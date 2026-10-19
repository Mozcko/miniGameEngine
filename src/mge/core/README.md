# src/mge/core/  →  target `mge_core`

The **headless core**: standard library + GLM (header‑only math) only. **No SDL, no GPU headers** – enforced
by the CI `core-only` job, which builds this target on a machine without SDL/GL installed
(`-DMGE_BUILD_MODULES=OFF`). See [ADR 0003](../../../docs/adr/0003-headless-core.md).

| File | Namespace | Purpose |
|------|-----------|---------|
| `Application.h/.cpp` | `mge` | `ApplicationConfig` + the runner: owns the loop and the modules, runs headless when no platform module is added |
| `Module.h` | `mge` | Interface for optional engine modules (`onInit`/`onFrameBegin`/`onFrameEnd`/`onShutdown`) |
| `Input.h/.cpp` | `mge` | `Key`/`MouseButton` enums, `InputState` (per‑frame edges, mouse, wheel), `ActionMap` (named actions and axes). SDL‑free: the platform module feeds it, tests inject it |
| `Random.h` | `mge` | Deterministic randomness: `Pcg32` (reference PCG32), `mix64`/`splitmix64`, coordinate hashes `hash2i`/`hash3i`, `toFloat01` |
| `math/Geometry.h/.cpp` | `mge` | `Aabb`, `Sphere`, `Plane`, `Ray`, `Frustum` (from view‑projection), ray tests (`intersectRayAabb/Sphere/Plane/Triangle`) |
| `ecs/` | `mge` | Lightweight ECS: `Entity`, `Registry` (sparse sets, views), `Transform`/`Parent`/`WorldTransform` + `TransformSystem`, `World` (systems per phase) |
| `Log.h/.cpp` | `mge` | Logging (`MGE_LOG_*` macros, levels, categories, sinks) and assertions (`MGE_ASSERT`, `MGE_VERIFY`) |
| `Assets.h/.cpp` | `mge` | `assetPath(rel)` / `assetRoot()` / `setAssetRoot()` / `executableDir()`: cwd‑independent asset paths (env `MGE_ASSET_DIR` → exe dir → exe/../assets → source tree) |
| `Clock.h/.cpp` | `mge` | Time source: `SteadyClock` (real time), `ManualClock` (tests / deterministic headless runs, optional auto‑step) |
| `Time.h` | `mge` | App‑owned time state (`app.time()`): scaled/unscaled delta, fixed delta, interpolation alpha, totals, time scale |


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
| `ApplicationConfig` | `title`, `width`, `height` (used by `PlatformModule`), `headless` (a hint for the game: add no platform module), `maxFrames` (0 = unlimited), `tickRate` (60), `maxFixedStepsPerFrame` (5), `maxFrameTime` (0.25 s), `vsync` (true; applied by the platform's GL context or by `GpuModule`, not by the core) |
| `addModule<T>(args…)` / `getModule<T>()` | Register modules before `run()`. Lookup is by type (`dynamic_cast`); returns `nullptr` if absent |
| `run()` | Init modules in order (stop at the first failure) → game `onInit` → loop → game `onShutdown` → shut down **only the initialised** modules in **reverse** order. Returns 0, or 1 if any init failed |
| Loop | modules' `onFrameBegin` · (break if a module requested stop) · measure the frame with the `Clock`, clamp to `maxFrameTime` · **`onFixedUpdate(fixedDt)` 0..N times** · `onUpdate(scaledDt)` · `onRender()` · modules' `onFrameEnd` · `maxFrames` check |
| `time()` / `setClock(clock)` | Read the app's `Time` (and `time().setTimeScale(s)`); replace the clock before `run()` |
| `requestStop()` | Ends the loop after the current frame. Called during `onInit` (or by a module's init), the loop never runs and shutdown follows directly |
| `setExitCode(code)` | Value `run()` returns after a normal shutdown (default 0). Headless checks use it to fail the process |
| `MGE_VSYNC` env | `0`/`1` overrides `config.vsync` (benchmarks: measure FPS without the monitor's limit); other values are ignored with a warning |
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

## Randomness – `<mge/core/Random.h>` (header‑only)

| Item | Use |
|------|-----|
| `Pcg32 rng(seed, stream)` | Fast, small, high‑quality generator: `nextU32()`, `nextFloat01()` ([0,1)), `nextBounded(n)` (unbiased), `range(min, maxExclusive)`, `chance(p)`. It's the reference PCG32, verified against pcg‑random.org's known‑answer vectors. Different `stream`s with the same seed are independent |
| `hash2i(x, z, seed)`, `hash3i(x, y, z, seed)` | Stateless coordinate hashes: same input → same value on every platform; neighbours are uncorrelated. Use them to seed per‑coordinate generators |
| `mix64(x)`, `splitmix64(state)` | 64‑bit mixers (`constexpr`); derive several seeds from one |
| `toFloat01(u32)` | u32 → float in [0, 1) using the top 24 bits |

Pattern: `Pcg32 rng(hash2i(worldX, worldZ, worldSeed), /*stream=*/1);`. Never use `rand()`.
Tests: `tests/core/test_random.cpp` (reference vectors, streams, bounds, χ² uniformity, hash
grid with no collisions).

---

## Geometry – `<mge/core/math/Geometry.h>`

| Type / function | Notes |
|-----------------|-------|
| `Aabb` | `min`/`max`; **empty by default** (min > max) so `expand`/`merge` build it up. `fromPoints`, `fromCenterExtents`, `center`, `extents` (half size), `size`, `contains` and `intersects` (both inclusive of the boundary) |
| `Sphere` | `contains`, and intersects a sphere or an `Aabb` (closest point) |
| `Plane` | `dot(normal, p) + d = 0`, unit normal; `fromPointNormal`, signed `distance` (positive on the normal side) |
| `Ray` | `origin` + normalized `direction`; `at(t)` |
| `Frustum::fromViewProjection(P * V, DepthRange)` | Gribb–Hartmann, 6 normalized planes pointing inward. `DepthRange::ZeroToOne` (default; the engine's projections, SDL GPU) or `MinusOneToOne` (OpenGL / `glm::perspective`): only the near plane differs. `contains(point)`, plus `intersects(Sphere / Aabb)`, which are conservative (may accept boxes just outside near corners) |
| `intersectRayAabb / Sphere / Plane / Triangle` | `std::optional<float>` distance `t ≥ 0`. Inside a box or sphere gives `t = 0`. Parallel → `nullopt`. The triangle test (Möller–Trumbore) is double‑sided |

Used for frustum culling through `CameraMatrices::frustum` (see Cameras below). Tests:
`tests/core/test_geometry.cpp`, `tests/core/test_camera.cpp`.

---

## ECS – `src/mge/core/ecs/`

```cpp
mge::Registry& registry = app.world().registry();
mge::Entity e = registry.create();
registry.add<mge::Transform>(e, mge::Transform{{0, 5, 0}});
registry.add<Velocity>(e, Velocity{1, 0, 0});              // any movable type is a component
app.world().addSystem(mge::SystemPhase::Fixed, "movement", [](mge::World& w, float dt) {
    w.registry().each<mge::Transform, Velocity>([dt](mge::Entity, mge::Transform& t, Velocity& v) {
        t.position += v.value * dt;
    });
});
```

| Piece | Behaviour |
|-------|-----------|
| `Entity` | 32‑bit index + 32‑bit generation. `destroy` bumps the generation, so stale handles fail `valid()`. Indices are recycled. `kNullEntity` is the null handle |
| `Registry` | `create`, `destroy` (removes all components), `valid`, `aliveCount`; per component type: `add` (replaces if present), `get`, `tryGet` (nullptr if absent), `has`, `remove`, `count` |
| Storage | One **sparse set** per type: dense contiguous arrays of entities and components, O(1) add/get/remove (swap‑and‑pop). Type ids come from a static counter |
| `view<A, B…>()` / `each<A, B…>(fn)` | Iterates the **smallest** pool and checks the others (pools looked up once per call). `fn(Entity, A&, B&…)` may modify components but must not add/remove these types during the iteration. Don't repeat a type in the list |
| `Transform` | `position`, `rotation` (quat), `scale`, `localMatrix()` (T·R·S) |
| `Parent` + `TransformSystem::update` | Computes `WorldTransform` (world = parent world · local), parents first. Invalid parents → root; cycles or depth > 256 → root + warning |
| `World` | `registry()` + systems per `SystemPhase` (Fixed / Update / Render), run in registration order. `Application` owns one (`app.world()`): **Fixed** runs in every fixed step before `onFixedUpdate`, **Update** before `onUpdate`, **Render** before `onRender` |

Benchmark (`bench/ecs_bench`, Release, this dev machine): 100k entities, `each<Position, Velocity>`
≈ **2.7 ns/entity**; create + 2.5 components ≈ 86 ns/entity. Tests: `tests/core/test_ecs.cpp`.

---

## Cameras – `<mge/core/ecs/Camera.h>`, `<mge/core/FlyController.h>`

Cameras are ECS components. They live in the core, so they're headless and testable without a GPU.

```cpp
mge::Registry& registry = world().registry();
mge::Entity camera = registry.create();
registry.add<mge::Transform>(camera);          // the camera looks down its local -Z, +Y up
registry.add<mge::Camera>(camera);             // perspective, 45°, near 0.1, far 1000, autoAspect

mge::FlyController fly({0, 20, 0});            // free fly + optional AABB collision
fly.update(dt, input);                         // CameraInput: move/look intents from the game's actions
fly.apply(registry.get<mge::Transform>(camera));

const mge::CameraMatrices m = mge::cameraMatrices(registry.get<mge::Transform>(camera).localMatrix(),
                                                  registry.get<mge::Camera>(camera), viewport());
renderer.setCamera(m.viewProjection);          // m.view, m.projection, m.position, m.frustum (culling)
```

| Item | Notes |
|------|-------|
| `Camera` | `projection` (`Perspective` / `Orthographic`), `fovY` (radians), `nearPlane`, `farPlane`, `orthoHeight`, `pixelSpace`, `aspect`, `autoAspect` |
| Projections | Z in **[0, 1]** (SDL GPU / Vulkan / D3D / Metal): `perspectiveRH_ZO`, `orthoRH_ZO`. NDC +Y is up |
| Pixel space | Orthographic with `pixelSpace = true`: (0, 0) is the **top‑left**, y grows downwards, 1 unit = 1 pixel (UI). A quad that is CCW in pixel coordinates is CW on screen |
| `cameraMatrices(world, camera, viewport)` | `view = inverse(world)`, `projection`, `viewProjection`, `position`, `frustum` (`DepthRange::ZeroToOne`) |
| `CameraSystem::update(registry, viewport)` | Sets `aspect` on every `autoAspect` camera; a zero viewport (minimised) keeps it. **The Application runs it before the Render phase**, so window resizes reach every camera with no game code |
| `Application::viewport()` / `setViewport()` | Size in pixels of the image being drawn. Starts at `config.width × config.height`; `GpuModule` updates it each frame from the swapchain |
| `FlyController` | Yaw/pitch from `lookX/lookY × sensitivity` (pitch clamped to ±89°), movement along the view (`speed`, `sprintMultiplier`), collisions axis by axis (X, Z, Y) against `halfExtents` around the position, so it slides along walls. `dt <= 0` (paused) does nothing. `apply(Transform&)` writes position + rotation |
| `CameraInput` | `moveForward`, `moveRight`, `moveUp` (−1..1), `sprint`, `lookX`, `lookY` (mouse pixels). Controllers never read keys, so they don't depend on the platform |

Replaces the legacy `Camera`/`Camera2D`/`Camera3D`/`FirstPersonCamera` of `mge_render` (#32), which
built OpenGL projections. Tests: `tests/core/test_camera.cpp`.

---

## Jobs – `<mge/core/Jobs.h>` (#56)

A simple thread pool for heavy, independent work, such as generating or meshing chunks:

```cpp
mge::JobSystem& jobs = app.jobs();                       // created on first use
std::future<int> result = jobs.submit([] { return heavyWork(); });
int value = result.get();                                // or poll: result.wait_for(0s) == ready
jobs.waitIdle();                                         // block until the queue is empty
```

| Item | Notes |
|------|-------|
| Threads | `hardware_concurrency − 1` (at least 1); the main thread stays for the game. `JobSystem(n)` sets it explicitly |
| Queue | One shared queue (mutex + condition variable), FIFO, no priorities or work stealing |
| `submit(fn)` | Returns a `std::future` of `fn`'s result. Exceptions travel in the future |
| Shutdown | `~JobSystem` lets running jobs finish, **drops** queued ones (their futures report `broken_promise`), and joins. `Application::run()` calls `waitIdle()` **before** `onShutdown`, so jobs never outlive the game data they use |
| Rules | Jobs must not touch main‑thread state without synchronisation. Copy what they need (e.g. `ChunkSnapshot`) or use const, stateless data (e.g. `TerrainGenerator::generate`) |

`mge_core` links `Threads::Threads`. Tests: `tests/core/test_jobs.cpp`, plus the `tsan` CI job
(ThreadSanitizer).

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
