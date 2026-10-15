# src/mge/core/  →  target `mge_core`

The **headless core**: standard library only (GLM later). **No SDL, no GPU headers** – enforced
by the CI `core-only` job, which builds this target on a machine without SDL/GL installed
(`-DMGE_BUILD_MODULES=OFF`). See [ADR 0003](../../../docs/adr/0003-headless-core.md).

| File | Namespace | Purpose |
|------|-----------|---------|
| `Application.h/.cpp` | `mge` | `ApplicationConfig` + the runner: owns the loop and the modules, runs headless when no platform module is added |
| `Module.h` | `mge` | Interface for optional engine modules (`onInit`/`onFrameBegin`/`onFrameEnd`/`onShutdown`) |
| `Input.h/.cpp` | `mge` | `Key`/`MouseButton` enums, `InputState` (per‑frame edges, mouse, wheel), `ActionMap` (named actions and axes). SDL‑free: the platform module feeds it, tests inject it |
| `Log.h/.cpp` | `mge` | Logging (`MGE_LOG_*` macros, levels, categories, sinks) and assertions (`MGE_ASSERT`, `MGE_VERIFY`) |
| `Clock.h/.cpp` | `mge` | Time source: `SteadyClock` (real time), `ManualClock` (tests / deterministic headless runs, optional auto‑step) |
| `Time.h` | `mge` | App‑owned time state (`app.time()`): scaled/unscaled delta, fixed delta, interpolation alpha, totals, time scale |

Upcoming in M1: RNG (#14), ECS (#16).

Tests: `tests/core/` → `mge_core_tests` (links only `mge_core`).


---

## Application runner – `mge::Application` + `mge::Module`

```cpp
#include <mge/core/Application.h>
#include <mge/platform/PlatformModule.h>   // only for windowed games

class MyGame : public mge::Application {
public:
    MyGame() : Application(makeConfig()) { addModule<mge::PlatformModule>(); }
protected:
    bool onInit() override { /* modules are ready (GL context exists) */ return true; }
    void onUpdate(float dt) override { /* ... */ if (done) requestStop(); }
    void onRender() override { /* draw; the platform module swaps buffers */ }
    void onShutdown() override { /* free GPU resources while the context exists */ }
};
mge::Application* mge::createApplication() { return new MyGame(); }
```

| Item | Behaviour |
|------|-----------|
| `ApplicationConfig` | `title`, `width`, `height` (used by `PlatformModule`), `headless` (a hint for the game: add no platform module), `maxFrames` (0 = unlimited), `tickRate` (60), `maxFixedStepsPerFrame` (5), `maxFrameTime` (0.25 s) |
| `addModule<T>(args…)` / `getModule<T>()` | Register modules before `run()`. Lookup is by type (`dynamic_cast`); returns `nullptr` if absent |
| `run()` | Init modules in order (stop at the first failure) → game `onInit` → loop → game `onShutdown` → shut down **only the initialised** modules in **reverse** order. Returns 0, or 1 if any init failed |
| Loop | modules' `onFrameBegin` · (break if a module requested stop) · measure the frame with the `Clock`, clamp to `maxFrameTime` · **`onFixedUpdate(fixedDt)` 0..N times** · `onUpdate(scaledDt)` · `onRender()` · modules' `onFrameEnd` · `maxFrames` check |
| `time()` / `setClock(clock)` | Read the app's `Time` (and `time().setTimeScale(s)`); replace the clock before `run()` |
| `requestStop()` | Ends the loop after the current frame |
| `setExitCode(code)` | Value `run()` returns after a normal shutdown (default 0). Headless checks use it to fail the process |
| `MGE_MAX_FRAMES` env | If `config.maxFrames == 0`, caps the frame count (invalid values are ignored with a warning). Used by CI to run real games for N frames |
| `Module` | `name()`, `onInit(app) -> bool`, `onFrameBegin(app)`, `onFrameEnd(app)`, `onShutdown(app)` (only if init succeeded) |

The core knows no concrete module. Platform and audio live in their own targets
(`PlatformModule`, `AudioModule`), so a program that adds none runs with no window, GPU or SDL.
Tests: `tests/core/test_application.cpp`.

---

## Input – `mge::InputState` + `mge::ActionMap`

```cpp
auto& platform = *getModule<mge::PlatformModule>();
platform.actions().bindAxis("MoveForward", mge::Key::W, mge::Key::S);
platform.actions().bind("Jump", mge::Key::Space);
platform.actions().bind("Jump", mge::MouseButton::Right);         // several bindings per action
// each frame:
if (platform.wasActionPressed("Jump")) { /* … */ }
float forward = platform.axis("MoveForward");                      // -1..1
mge::Vec2f look = platform.input().mouseDelta();
```

| Item | Behaviour |
|------|-----------|
| `InputState::beginFrame()` | Saves the previous key/button state and resets mouse delta and wheel. The platform module calls it before applying the frame's events |
| `isDown` / `wasPressed` / `wasReleased` | Level / pressed this frame / released this frame, for `Key` and `MouseButton`. Press + release within one frame leaves no edge |
| `mousePosition()`, `mouseDelta()`, `wheel()` | Position persists; delta and wheel accumulate per frame |
| `clear()` | Releases everything (focus lost) |
| `ActionMap::bind(action, Key / MouseButton)` | Any binding drives the action. `wasPressed` fires only when the first binding goes down; `wasReleased` only when the last one goes up |
| `ActionMap::bindAxis(axis, positive, negative)` | Sums +1/−1 over all pairs, clamped to [−1, 1] |

Tests: `tests/core/test_input.cpp`.

---

## Logging – `mge::Log` + macros

```cpp
#include <mge/core/Log.h>
MGE_LOG_INFO("Audio", "Cargado ", path, " (", bytes, " bytes)");   // args joined with operator<<
MGE_LOG_ERROR("Shader", "Falló la compilación de ", file);
if (!MGE_VERIFY(window != nullptr, "sin ventana")) return false;   // always evaluated, logs on failure
MGE_ASSERT(index < size, "índice fuera de rango");                  // Debug only; failure → handler (abort)
```

| Feature | Behaviour |
|---------|-----------|
| Levels | `Trace`, `Debug`, `Info`, `Warn`, `Error`, `Fatal` (+ `Off`). `Fatal` only logs – the caller decides how to stop. |
| Output | Default sink writes `[seconds] [LEVEL] [Category] message` to **stderr**, thread‑safe (mutex). |
| Runtime filter | `Log::setMinLevel(level)` – default `Trace` in Debug, `Info` in Release. Filtered messages don't evaluate their arguments. |
| Compile‑time filter | `MGE_LOG_COMPILE_LEVEL` (0 = Trace … 5 = Fatal). Default 0 in Debug, 2 in Release (`NDEBUG`): `MGE_LOG_TRACE/DEBUG` compile to nothing. Error/Fatal are never stripped. |
| Custom sink | `Log::setSink(fn)` (tests capture records; `nullptr` restores stderr). The sink runs under the log mutex – don't log from inside it. |
| Assertions | `MGE_ASSERT(cond, msg...)` – Debug only; logs `Fatal` then calls `Log::setAssertHandler` handler (default `std::abort`). `MGE_VERIFY(cond, msg...)` – any build; returns `cond`, logs `Error` when false. |

Rule: engine and game code never use `std::cout`, `std::cerr`, `printf` or `exit()` – use the
macros and return errors to the caller.

---

## Time – fixed timestep (`mge::Time`, `mge::Clock`)

Each frame the loop measures the real frame time with the app's `Clock` and clamps it to
`maxFrameTime` (0.25 s). It multiplies by the time scale and adds the result to an accumulator.
`onFixedUpdate(fixedDt)` then runs once per whole `1 / tickRate` in the accumulator, up to
`maxFixedStepsPerFrame` (5) times. Any extra backlog is **dropped**, which prevents the
"spiral of death".

| Use | For |
|-----|-----|
| `onFixedUpdate(fixedDt)` | Deterministic simulation (physics, gameplay rules): the same `fixedDt` every call, independent of FPS |
| `onUpdate(dt)` | Per‑frame logic with the **scaled** delta (camera, input, streaming). 0 while paused |
| `time().unscaledDelta()` / `unscaledElapsed()` | Real time: UI, animations that keep running while paused, FPS |
| `time().alpha()` | `[0, 1)` progress towards the next fixed step, to interpolate rendering between simulation states |
| `time().setTimeScale(s)` | 1 = normal, 0 = pause (no fixed steps, scaled delta 0), 0.5 = slow motion. Negative values clamp to 0 |
| `time().frameCount()`, `fixedStepCount()`, `elapsed()` | Counters (`elapsed` is scaled time) |

Determinism: `setClock(std::make_unique<ManualClock>(1.0 / 60.0))` makes every frame last exactly
1/60 s. Same inputs then always give the same number of fixed steps and the same state. The
accumulator uses `double` with a 1 ns tolerance, so a step isn't lost to rounding when summing,
say, 4 × 1/240 s.

Tests: `tests/core/test_time.cpp` (frame‑rate independence, accumulation and alpha, the step cap,
pause, determinism).
