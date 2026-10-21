# CLAUDE.md – context for AI assistants

Read this first. Deeper docs: `README.md` (index), `docs/ARCHITECTURE.md`, `docs/KNOWN_ISSUES.md`,
and a `README.md` inside every folder of `src/` and `assets/`.

## Direction (read before changing anything)
- Goal: **headless C++ core** + optional modules; games made from **pure math/code**, shipped as a
  games built from **pure math/code** *or* with regular asset files (both supported; ADR 0004). Full plan: `docs/ROADMAP.md` (GitHub milestones M0–M8,
  issues #1–#67, epics #68–#76 on Mozcko/miniGameEngine).
- **GPU API decision: SDL3 GPU API (Vulkan/Metal/D3D12). Do NOT build new features on OpenGL** – the
  GL renderer is legacy and gets deleted after parity (#40). Platform is on SDL3 (#9 done).
- Workflow & conventions: `CONTRIBUTING.md` (branch `type/<issue>-slug`, conventional commits, one issue
  per PR with "Closes #N", merge only when CI is green). Decisions: `docs/adr/` (0001 SDL3 GPU,
  0003 headless core, 0004 procedural‑first/assets optional – supersedes 0002) – supersede with a new ADR, never edit accepted ones.

## What this is (current state, pre-migration)
- C++17 mini game engine: **SDL3** (window/input/events/audio), **SDL3 GPU API** (Vulkan today; Metal/D3D12 later) for
  rendering, GLM math, stb_image (textures). OpenGL/GLEW were removed in #40. Linux/Fedora dev box, CMake build. SDL3 is
  used from the system (`SDL3-devel`) or built from source via FetchContent (pinned 3.4.18).
- Demo client: `game/SandboxApp.cpp` – Minecraft‑like voxel world on `mge_voxel` (16³ chunks, 4 tall; noise terrain with
  biomes and caves, trees/flowers, animated water/lava; F1 performance overlay, FPS also in the Debug log).
- Owner writes code comments in **Spanish**; keep that style in code. Docs are in English.

## Build & run
```bash
cmake --preset dev && cmake --build --preset dev   # Debug + warnings → build/ (also: release, ci)
scripts/run.sh               # build + run from anywhere (or ./build/sandbox, or --target run)
```
Must build **warning‑free** (`-Wall -Wextra -Wshadow`; CI uses `-Werror` via the `ci` preset). Default build type is Release. CI: GitHub Actions (`.github/workflows/ci.yml`, GCC + Clang, `ci` preset) must be green before merging.
Sanitizers: `asan` preset (ASan+UBSan; locally use Clang – GCC's libasan isn't installed here); `tsan` preset (ThreadSanitizer, clang, headless modules).
Smoke tests (CTest): `examples/headless_smoke` (no modules, 1000 frames), `examples/platformer2d` (the 2D path, #45) and the sandbox 120 frames offscreen (label `gpu`, uses `MGE_MAX_FRAMES`). CI jobs: gcc, clang, sanitizers, tsan, core-only, lint.
Tests: doctest + CTest in `tests/` (`ctest --preset dev`), headless – no GPU/display needed.
`CMakeLists.txt` uses `file(GLOB_RECURSE ...)` – new .cpp files need a CMake re‑configure.

## Layout & targets
- `src/mge/<module>/` = one static lib each: `mge_core` (headless, **no SDL/GPU** – CI `core-only` job
  builds it with `-DMGE_BUILD_MODULES=OFF`, together with the headless `mge_procgen`/`mge_voxel`), `mge_platform` (PlatformModule),
  `mge_gpu` (GpuModule, SDL3 GPU), `mge_graphics` (Texture2D, MeshRenderer), `mge_synth` (headless audio: Mixer, samples), `mge_audio` (AudioModule: the mixer on the SDL audio device), `mge_main` (main(), core‑only).
- `game/` = `sandbox_world` lib (SandboxBlocks/SandboxTerrain on `mge_voxel`, headless; namespace `sandbox`) + `sandbox` exe (written to the build dir root).
- Tests: `tests/<module>/` → `mge_<module>_tests` (core, procgen, voxel, platform, gpu), `tests/game/` → `game_tests`.
- Explicit source lists per target CMakeLists (no globbing) – add new files there.

## Architecture in 10 lines
1. `src/mge/app/Main.cpp` (`mge_main`) owns `main()` → `mge::createApplication()` (game) → `return app->run()`.
2. `mge::Application` (core, `<mge/core/Application.h>`) = runner + `Module`s. The game adds modules in its
   ctor (`addModule<PlatformModule>()`, `addModule<AudioModule>()`); none = headless. Config: `ApplicationConfig`.
3. `run()`: modules `onInit` in order → game `onInit()` (bool) → loop { modules `onFrameBegin` (Platform polls
   SDL, QUIT → stop, event callback), `onUpdate(dt)`, `onRender()`, modules `onFrameEnd` (swap) } → game
   `onShutdown()` → initialised modules `onShutdown` in reverse. Returns 0 / 1 (init failed).
4. Time (#12): `onFixedUpdate(fixedDt)` at `tickRate` (max 5/frame), `onUpdate(scaledDt)`, `app.time()` (alpha,
   unscaled delta, `setTimeScale`). `setClock(make_unique<ManualClock>(1/60.))` = deterministic. No static `Time`.
   Game stops with `requestStop()`. Input (#15): core `InputState`/`ActionMap` (`<mge/core/Input.h>`), owned by
   `PlatformModule` (`actions()`, `wasActionPressed`, `axis`, `setCursorLocked`); camera controllers take a `CameraInput` struct.
   `setEventCallback` is only for window events (resize/focus).
5. No static singletons left (input, time and audio are owned by the app/modules; `AudioManager` was removed in #46).
6. Rendering = SDL3 GPU (`mge_gpu` + `mge_graphics`): `device.uploadMesh(MeshData)` → `GpuMesh` (`GpuVertex{pos, normal, color, uv}`),
   `MeshRenderer::begin/setCamera/draw/end` (opaque, then transparent).
7. Shaders are GLSL compiled to SPIR‑V at build time and embedded (`mge_add_shaders`); no shader files at runtime.
8. `Texture2D` (move‑only) from file or pixels; stored flipped so engine UVs stay GL‑style (v up, (0,0) = image bottom‑left).
9. Cameras (core, #32): ECS `Camera` component + `Transform`; `cameraMatrices(world, camera, viewport())` (Z in [0,1], `.frustum` for culling);
   `CameraSystem` sets `autoAspect` from `Application::viewport()` (set by GpuModule) before the Render phase; `FlyController` = free fly + collisions.
10. Game rendering: `RenderQueue` (#31) → passes Opaque, Cutout (alpha test, leaves/plants), Transparent (blend, no depth write);
   opaque/cutout front‑to‑back, transparent back‑to‑front. Blocks choose their pass with `BlockDesc::layer` (`RenderLayer`).
   Lighting (#33): `renderer.setLighting(Lighting)` (sun + hemisphere ambient + linear fog; default `Lighting::unlit()`),
   `setCamera(viewProjection, cameraPosition)`. Shaders `mesh.frag`/`mesh_cutout.frag` duplicate the lighting code (no GLSL #include).

## Conventions
- **One engine namespace `mge`** (game code: `sandbox`). Naming, enforced by
  `.clang-tidy` in the CI `lint` job: types `PascalCase`, functions/methods `camelCase`,
  private/protected members `m_camelCase`, static members `s_camelCase`, static constants
  `kPascalCase`, public data fields `camelCase`, macros `UPPER_CASE`.
- Formatting: `.clang-format`, checked in CI with clang-format **22.1.8**. Before committing, run
  **`scripts/lint.sh --fix`**: the same format + naming checks as CI, including untracked new files.
  It needs `cmake --preset ci` first.
- Include roots `src/` and the repo root (for `game/`); always root‑relative: `#include <mge/core/Time.h>`, `#include <game/SandboxTerrain.h>`.
- Block face index: 0 Front(+Z), 1 Back(−Z), 2 Left(−X), 3 Right(+X), 4 Top(+Y), 5 Bottom(−Y).
- Block (x,y,z) occupies [x, x+1)³ → world→block is `mge::blockAt(p)` (= floor).
- Tileset: 512×512 px, 32 px tiles, 16 cols, tile index row‑major from the **top‑left**.

## Jobs (#56)
`app.jobs()` = `JobSystem` (created on first use; hardware threads − 1): `submit(fn) -> std::future`, `waitIdle()`. `run()` waits for
all jobs **before** `onShutdown`. Workers must not touch main-thread state: voxel generation runs `generator()->generate` (const,
stateless) then `VoxelWorld::insertChunk` on the main thread; meshing runs on a `ChunkSnapshot::capture(world, coord)` copy
(chunk + 1-block border) via `buildChunkMesh(snapshot, registry, options)`. `ChunkStreamer(world, StreamingSettings, &jobs())` does
all of that: `update(center)` → `{meshes to upload, unloaded coords}`; stale results (unloaded / edited since) are discarded.

## Procedural textures (#34)
`mge::Image` (RGBA8, `writePng` without deps) + `mge::texgen::` generators (noise, bricks, planks, ore, stripes, rings, plant, leaves…).
Sandbox: `buildProceduralAtlas()` (same tiles as the PNG); `M` toggles, `SANDBOX_ATLAS=procedural` loads no image.
Colour‑only meshes: `MeshMaterial{detailScale, detailStrength}` per draw (world‑space noise in the shader).

## Text, 2D UI and overlay (#41–#43)
`mge::Font` (procgen, headless): glyph atlas (`Image`) + metrics. `Font::builtin()` = embedded 8×8 font (no files, nearest, integer
scales); `loadTtfFont/loadTtfFontFile` (mge_graphics, stb_truetype, optional). UTF‑8 in, unknown characters → `?`.
2D UI (#42): `Renderer2D ui(device, format)`; per frame `ui.begin(w, h, round(frame->displayScale))`, `rect/rectOutline/line/image/
text/pushClip`, then `ui.flush(cmd, swapchain)` after the 3D pass (loads, doesn't clear). Logical pixels, origin top‑left, y down.
`Batch2D` = the CPU part (testable); untextured quads use `uv = kNoTexture`, so rects and text share one draw call.
Overlay (#43): `DebugOverlay` (`addFrame(ms, frame->acquireMs)`, `setCounter(name, value)`, `draw(ui)`); no GPU timestamps in SDL GPU,
so it shows CPU vs swapchain wait. Sandbox: F1 (on in Debug, off in Release; `SANDBOX_DEBUG=1` turns it on).

## Audio (M5)
`mge_synth` (headless, always built): `Mixer` — game thread enqueues POD commands in a lock‑free `SpscQueue` (`playTone`,
`playSample`, `stopAll`; volume/mute are atomics); the audio thread runs `render(out, frames)`: **no allocations, locks or logging**,
32 preallocated voices (oldest stolen), immutable samples in a fixed bank (never freed while playing). Float32 stereo 48 kHz.
Synth (#47): `Instrument` (wave, `Adsr`, `Filter`, vibrato, pitch slide, delay send) → `playNote(instrument, hz, velocity, duration, bus)` →
`VoiceHandle` (`releaseNote`/`stopNote`); buses `Effects`/`Music`; shared `Delay`; `softClip` on the master; `midiToHz`.
SFX (#48): `SfxParams` (sfxr‑style) → `renderSfx` → Sample; `audio->addSfx(mge::sfx::jump())`; presets jump/coin/hurt/explosion/powerUp/click,
`sfx::randomize(seed)`, `mutate`. Platformer: synthesized by default, `S` toggles the WAVs (`PLATFORMER_SOUNDS=wav`).
Offline (#50): `renderOffline(mixer, seconds)`, `writeWav`, `mge::analysis::` (rms, zeroCrossingHz, toneAmplitude, spectralCentroid, onsets)
→ audio tests compare features with tolerances, not samples. Listen: `examples/audio_gallery` → `<build>/gallery/audio/*.wav` (+ spectrogram PNGs).
`AudioModule` (mge_audio): SDL3 device stream + callback; `loadWav` (→ `assetPath`, converted to float32), `play(id, gain, pan)`,
`playTone`, `setMasterVolume`, `setMuted` (`MGE_MUTE=1`). No device / `SDL_AUDIODRIVER=dummy` → silent, never an error.

## Debug draw (#38)
`DebugDraw debug(device)`: `aabb/sphere/axes/line/frustum` then `debug.flush(renderer)` inside the render pass (after the queue).
Sandbox: F3 chunk bounds + collision box, F4 freeze culling frustum; `SANDBOX_DEBUG=1` starts with both (smoke test).

## ECS
`app.world()` = `Registry` (sparse sets; `create/destroy/add/get/tryGet/has/remove`, `each<A,B>(fn)`) + systems
per `SystemPhase` (Fixed in each fixed step, Update, Render). `Transform`/`Parent` → `TransformSystem` → `WorldTransform`.
The sandbox doesn't use the ECS yet (cameras/chunks are plain members). Benchmark: `bench/ecs_bench`.

## Umbrella header & guide
`#include <mge/mge.h>` = core + linked modules (`MGE_HAS_PROCGEN/VOXEL/PLATFORM/GPU/RENDER/AUDIO` come from the module targets).
Newcomer guide: `docs/FIRST_GAME.md` (keep it in sync with `templates/`).

## Editor
`.vscode/`: tasks (Build dev = Ctrl+Shift+B, Run game, Test, ASan, Lint, New game) and gdb launch configs
(Debug sandbox, offscreen, core/game tests with a doctest filter). All paths assume the `dev` preset (`build/`).

## Games
Declare every program with `mge_add_game(<name> SOURCES … [LIBRARIES …] [MODULES …] [HEADLESS] [SMOKE_FRAMES n])`
(`cmake/MgeGame.cmake`): it links main + modules and adds `run-<name>` and a smoke test. New game: `scripts/new-game.sh <name> [--headless]`
→ `examples/<name>/` from `templates/` (`*.in`, CI builds both templates).

## Procgen
`mge_procgen` (headless, always built): `MeshData` (SoA, all attributes same length, `validate()`, `writeObj`).
GPU upload: `device.uploadMesh(meshData)` (mge_gpu). Visual checks: `examples/procgen_gallery` → `<build>/gallery/*.obj`.

## Voxel
`mge_voxel` (headless, always built): `BlockRegistry` (id 0 = air; `BlockDesc` colour and/or atlas tiles), 16³ chunks stacked in
all axes (`ChunkCoord`, `chunkOf`/`localOf`/`blockAt`; block (x,y,z) occupies [x,x+1)), `VoxelWorld` (+ `TerrainGenerator`,
dirty flags; loading dirties neighbours), `buildChunkMesh` → `{opaque, transparent}` MeshData in chunk‑local space (greedy for colour‑only
cubes, optional AO). Generators must be stateless/order‑independent (sandbox trees replay neighbour columns). The game only registers blocks and
implements a generator.

## GPU (SDL3 GPU, M3 in progress)
`mge_gpu`: `PlatformModule` + `GpuModule` (added after it) → `frame()` {cmd, swapchain, w, h} each frame;
`GpuDevice` factories return move‑only handles (`GpuBuffer`, `GpuTexture`, …; release before the device); `uploadMesh(MeshData)` →
`GpuVertex{pos, normal, color, uv}`; `downloadTexture` for tests. GPU tests run offscreen (lavapipe in CI, `MGE_REQUIRE_GPU=1`; a "Validation Error" in a GPU test's output fails it, `MGE_REQUIRE_VALIDATION=1`).
Shaders: GLSL → SPIR‑V at build time, `mge_add_shaders(target SHADERS x.vert y.frag)` → `#include <shaders/x_vert.h>` →
`device.createShader(mge::shaders::kXVert, ShaderResources{…})`; bindings: vertex set0 tex / set1 UBO, fragment set2 tex / set3 UBO.
Pipelines: `GraphicsPipelineDesc` → `device.createGraphicsPipeline(desc)`. glslang: system or fetched from source.
Render tests: `tests/render` golden PNGs (`MGE_UPDATE_GOLDEN=1` to regenerate, then look at them). SDL GPU NDC is Y‑up with Z in [0,1]:
use `glm::perspectiveRH_ZO`; CCW front faces as in MeshData.
`mge_graphics`: `Texture2D::fromFile/fromPixels`, `MeshRenderer` (README in src/mge/graphics). Default `mge_add_game` MODULES:
`platform gpu graphics audio`.
Vsync: `ApplicationConfig::vsync` (env `MGE_VSYNC`), `GpuModule::setVsync`; depth: `device.depthFormat()` (D32/D24) + `createDepthTarget`.

## Assets
Always `mge::assetPath("shaders/x.vert")` (`<mge/core/Assets.h>`), never `"../assets/..."`; the root is logged at startup
(override: `MGE_ASSET_DIR`).

## Randomness
`<mge/core/Random.h>`: `Pcg32 rng(hash2i(x, z, seed), stream)`; never `rand()`. The sandbox world seed
defaults to `SandboxTerrain::kDefaultSeed` (1337).

## Logging
`#include <mge/core/Log.h>` → `MGE_LOG_INFO/WARN/ERROR/FATAL("Category", "text ", value)`, `MGE_ASSERT`
(Debug), `MGE_VERIFY` (always, returns bool). No `std::cout`/`cerr`/`printf`/`exit()` anywhere: log
and return an error instead. `Window::isValid()`, `GpuDevice::create` (nullptr), `Texture2D` (invalid) report failure.

## Gotchas you will trip on (details in docs/KNOWN_ISSUES.md)
- Chunks stream through `ChunkStreamer` (#57): load/mesh on worker threads, unload with hysteresis, pooled storage. Frustum culling via `CameraMatrices::frustum` (`Frustum::fromViewProjection`, `DepthRange::ZeroToOne` by default), drawing only.
- Headless runs: `SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy` (Vulkan through `VK_EXT_headless_surface`).
- GPU resources (`GpuMesh`, `Texture2D`, pipelines) must be released in `onShutdown`, before `GpuModule` destroys the device.
- `assets/levels` + `assets/sounds` are kept as 2D test data: `examples/platformer2d` (#45) loads them (`music/` waits for M5). Don't delete them.
