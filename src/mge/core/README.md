# src/mge/core/  →  target `mge_core`

The **headless core**: standard library only (GLM later). **No SDL, no GPU headers** – enforced
by the CI `core-only` job, which builds this target on a machine without SDL/GL installed
(`-DMGE_BUILD_MODULES=OFF`). See [ADR 0003](../../../docs/adr/0003-headless-core.md).

| File | Namespace | Purpose |
|------|-----------|---------|
| `Application.h/.cpp` | `mge` | `ApplicationConfig` + the runner: owns the loop and the modules, runs headless when no platform module is added |
| `Module.h` | `mge` | Interface for optional engine modules (`onInit`/`onFrameBegin`/`onFrameEnd`/`onShutdown`) |
| `Log.h/.cpp` | `mge` | Logging (`MGE_LOG_*` macros, levels, categories, sinks) and assertions (`MGE_ASSERT`, `MGE_VERIFY`) |
| `Time.h/.cpp` | `mge` | Static clock (`std::chrono::steady_clock`) with time scale and hitch clamping |

Upcoming in M1: fixed‑timestep clock (#12),
input state (#15), RNG (#14), ECS (#16).

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
| `ApplicationConfig` | `title`, `width`, `height` (used by `PlatformModule`), `headless` (a hint for the game: add no platform module), `maxFrames` (0 = unlimited; tests and headless runs) |
| `addModule<T>(args…)` / `getModule<T>()` | Register modules before `run()`. Lookup is by type (`dynamic_cast`); returns `nullptr` if absent |
| `run()` | Init modules in order (stop at the first failure) → game `onInit` → loop → game `onShutdown` → shut down **only the initialised** modules in **reverse** order. Returns 0, or 1 if any init failed |
| Loop | `dt` from `steady_clock` · modules' `onFrameBegin` · (break if a module requested stop) · `onUpdate(dt)` · `onRender()` · modules' `onFrameEnd` · `maxFrames` check |
| `requestStop()` | Ends the loop after the current frame |
| `setExitCode(code)` | Value `run()` returns after a normal shutdown (default 0). Headless checks use it to fail the process |
| `MGE_MAX_FRAMES` env | If `config.maxFrames == 0`, caps the frame count (invalid values are ignored with a warning). Used by CI to run real games for N frames |
| `Module` | `name()`, `onInit(app) -> bool`, `onFrameBegin(app)`, `onFrameEnd(app)`, `onShutdown(app)` (only if init succeeded) |

The core knows no concrete module. Platform and audio live in their own targets
(`PlatformModule`, `AudioModule`), so a program that adds none runs with no window, GPU or SDL.
Tests: `tests/core/test_application.cpp`.

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

## `mge::Time`

All‑static clock. Must be driven by the game: call `Time::init()` once and `Time::update()`
once per frame (SandboxApp does this at the start of `onUpdate`).

| Method | Returns / effect |
|--------|------------------|
| `init()` | Stores the current `steady_clock` time. |
| `update()` | Computes raw dt; **clamps to 0.1 s** (protects against window drags / breakpoints); `deltaTime = raw * timeScale`. |
| `getDeltaTime()` | Scaled dt – use for gameplay (0 when paused). |
| `getUnscaledDeltaTime()` | Real dt – use for UI, animations that run while paused. |
| `getTimeScale()` / `setTimeScale(s)` | 1 = normal, 0 = paused, 0.5 = slow motion. |
| `advance(raw)` | Applies a raw delta: clamps to [0, `kMaxDeltaTime` = 0.1 s], then scales. `update()` calls it with the real clock; tests call it directly. |
