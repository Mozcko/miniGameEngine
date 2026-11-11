# CLAUDE.md – context for AI assistants

Read this first. Deeper docs: `README.md` (index), `docs/ARCHITECTURE.md`, `docs/KNOWN_ISSUES.md`,
and a `README.md` inside every folder of `src/` and `assets/`.

## Direction (read before changing anything)
- Goal: **headless C++ core** + optional modules; games built from **pure math/code** *or* with regular asset files
  (both supported; ADR 0004). Full plan: `docs/ROADMAP.md` (GitHub milestones M0–M10, issues #1–#67, #96–#107 and #160–#164,
  epics #68–#76, #101, #107, #164 on Mozcko/miniGameEngine). **M0–M8 are done (engine MVP); next is M9 (mge CLI, post‑1.0), then
  M10 (C API + Python bindings).**
- **GPU API: SDL3 GPU API (Vulkan/Metal/D3D12). Never add OpenGL** – the GL renderer was deleted in #40. Platform is SDL3.
- Workflow & conventions: `CONTRIBUTING.md` (branch `type/<issue>-slug`, conventional commits, one issue
  per PR with "Closes #N", merge only when CI is green). Decisions: `docs/adr/` (0001 SDL3 GPU,
  0003 headless core, 0004 procedural‑first/assets optional – supersedes 0002, 0005 wizard TUI, 0006 language bindings) – supersede with a new ADR, never edit accepted ones.

## What this is (current state)
- C++17 mini game engine: **SDL3** (window/input/events/audio), **SDL3 GPU API** (Vulkan today; Metal/D3D12 later) for
  rendering, GLM math, stb_image (textures). OpenGL/GLEW were removed in #40. Linux/Fedora dev box, CMake build. SDL3 is
  used from the system (`SDL3-devel`) or built from source via FetchContent (pinned 3.4.18).
- Showcase: `game/SandboxApp.cpp` – zero‑asset voxel sandbox on `mge_voxel` + `mge_physics` (16³ chunks with LOD rings; noise
  terrain with biomes and caves, trees/flowers, animated water/lava; procedural atlas, sky and day/night; walk/fly character,
  block break/place, thrown rigid bodies; synthesized SFX and music; HUD + F1 profiler overlay). Second demo:
  `examples/sdf_glide` (SDF raymarched game). Guide game: `examples/guide_game` (walkthrough of `docs/GUIDE.md`).
- Owner writes code comments in **Spanish**; keep that style in code. Docs are in English.

## Build & run
```bash
cmake --preset dev && cmake --build --preset dev   # Debug + warnings → build/ (also: release, ci)
scripts/run.sh               # build + run from anywhere (or ./build/sandbox, or --target run)
```
Must build **warning‑free** (`-Wall -Wextra -Wshadow`; CI uses `-Werror` via the `ci` preset). Default build type is Release. CI: GitHub Actions (`.github/workflows/ci.yml`, GCC + Clang, `ci` preset) must be green before merging.
Sanitizers: `asan` preset (ASan+UBSan; locally use Clang – GCC's libasan isn't installed here); `tsan` preset (ThreadSanitizer, clang, headless modules).
Smoke tests (CTest): `examples/headless_smoke` (no modules, 1000 frames), `examples/platformer2d` (the 2D path, #45), `sdf_scene`,
`sdf_glide`, `guide_game` and the sandbox 120 frames offscreen (label `gpu`, uses `MGE_MAX_FRAMES`).
CI jobs: gcc, clang, sanitizers, tsan, core-only, lint, **bench** (warns on regressions vs main), **size** (fails above
`cmake/size-budget.txt`), **zero-asset** (games run from an empty dir, `strace` proves no `assets/` access).
Tests: doctest + CTest in `tests/` (`ctest --preset dev`), headless – no GPU/display needed.
No globbing: every target lists its sources explicitly – add new files to that target's CMakeLists.txt.

## Layout & targets
- `src/mge/<module>/` = one static lib each: `mge_core` (headless, **no SDL/GPU** – CI `core-only` job
  builds it with `-DMGE_BUILD_MODULES=OFF`, together with the headless `mge_procgen`/`mge_voxel`/`mge_physics`/`mge_synth`),
  `mge_physics` (headless: swept voxel collision, character controller, rigid bodies), `mge_platform` (PlatformModule),
  `mge_gpu` (GpuModule, SDL3 GPU), `mge_graphics` (Texture2D, MeshRenderer), `mge_synth` (headless audio: Mixer, samples), `mge_audio` (AudioModule: the mixer on the SDL audio device), `mge_main` (main(), core‑only).
- Each engine library provides an alias `mge::<module>` (e.g. `mge::core`, `mge::platform`, `mge::main`). Out-of-tree consumers can consume the engine via installed package (`find_package(mge 0.9)`) or `FetchContent` (#103, `docs/USING_THE_ENGINE.md`).
- `game/` = `sandbox_world` lib (SandboxBlocks/SandboxTerrain/SandboxSounds on `mge_voxel`, headless; namespace `sandbox`) + `sandbox` exe (written to the build dir root).
- Tests: `tests/<module>/` → `mge_<module>_tests` (core, procgen, synth, voxel, physics, platform, audio, gpu), `tests/render` →
  `render_tests` (goldens), `tests/game/` → `game_tests`, `tests/examples/` → `platformer_tests`, `sdf_glide_tests`.
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
   `CameraSystem` sets `autoAspect` from `Application::viewport()` (set by GpuModule) before the Render phase; `FlyController` = free‑fly
   spectator camera (core); the player is a `CharacterController` (mge_physics, see Physics).
10. Game rendering: `RenderQueue` (#31) → passes Opaque, Cutout (alpha test, leaves/plants), Transparent (blend, no depth write);
   opaque/cutout front‑to‑back, transparent back‑to‑front. Blocks choose their pass with `BlockDesc::layer` (`RenderLayer`).
   Lighting (#33): `renderer.setLighting(Lighting)` (sun + hemisphere ambient + linear fog; default `Lighting::unlit()`),
   `setCamera(viewProjection, cameraPosition)`. Shaders share lighting, SDF, and raymarching libraries via `#include` (#36).

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
Sandbox: `buildProceduralAtlas()` (same tiles as the PNG) is the **default**; the PNG is opt‑in (`SANDBOX_ATLAS=png` or `M`; missing
PNG → stays procedural with one warning).
Colour‑only meshes: `MeshMaterial{detailScale, detailStrength}` per draw (world‑space noise in the shader).

## Text, 2D UI and overlay (#41–#43)
`mge::Font` (procgen, headless): glyph atlas (`Image`) + metrics. `Font::builtin()` = embedded 8×8 font (no files, nearest, integer
scales); `loadTtfFont/loadTtfFontFile` (mge_graphics, stb_truetype, optional). UTF‑8 in, unknown characters → `?`.
2D UI (#42): `Renderer2D ui(device, format)`; per frame `ui.begin(w, h, round(frame->displayScale))`, `rect/rectOutline/line/image/
text/pushClip`, then `ui.flush(cmd, swapchain)` after the 3D pass (loads, doesn't clear). Logical pixels, origin top‑left, y down.
`Batch2D` = the CPU part (testable); untextured quads use `uv = kNoTexture`, so rects and text share one draw call.
Overlay (#43): `DebugOverlay` (`addFrame(ms, frame->acquireMs)`, `setCounter(name, value)`, `draw(ui)`); no GPU timestamps in SDL GPU,
so it shows CPU vs swapchain wait, plus the profiler table (`setProfile`, see Profiling). Sandbox: F1 (on in Debug, off in Release;
`SANDBOX_DEBUG=1` turns it on); F11 toggles the game HUD (crosshair, hotbar, mode, hour).

## Audio (M5)
`mge_synth` (headless, always built): `Mixer` — game thread enqueues POD commands in a lock‑free `SpscQueue` (`playTone`,
`playSample`, `stopAll`; volume/mute are atomics); the audio thread runs `render(out, frames)`: **no allocations, locks or logging**,
32 preallocated voices (oldest stolen), immutable samples in a fixed bank (never freed while playing). Float32 stereo 48 kHz.
Synth (#47): `Instrument` (wave, `Adsr`, `Filter`, vibrato, pitch slide, delay send) → `playNote(instrument, hz, velocity, duration, bus)` →
`VoiceHandle` (`releaseNote`/`stopNote`); buses `Effects`/`Music`; shared `Delay`; `softClip` on the master; `midiToHz`.
SFX (#48): `SfxParams` (sfxr‑style) → `renderSfx` → Sample; `audio->addSfx(mge::sfx::jump())`; presets jump/coin/hurt/explosion/powerUp/click,
`sfx::randomize(seed)`, `mutate`. Platformer: synthesized by default, `S` toggles the WAVs (`PLATFORMER_SOUNDS=wav`).
Music (#49): `Song(bpm, stepsPerBeat)` + `addInstrument` + `pattern("C4 - . E4 | x .")` + `addTrack(instrument, {patterns})`,
`audio->playSong(audio->addSong(song), fade)` (loops, sample‑accurate, crossfade, Music bus); `mge::songs::platformerTheme/sandboxAmbient`.
Sandbox: `songs::sandboxAmbient` by day, `songs::sandboxNight` at night (4 s crossfade), **N** mutes music; SFX in
`game/SandboxSounds` (footsteps per material with 4 variants, jump, land, break, place, splash, throw – no files).
Platformer: theme (off when `S` selects the WAVs).
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
Sandbox: player (`CharacterController`), camera (`CharacterCamera`) and thrown bodies (`RigidBody`) are entities; chunks are plain
members. Benchmarks: `bench/mge_bench` (see Profiling).

## Umbrella header & guide
`#include <mge/mge.h>` = core + linked modules (`MGE_HAS_PROCGEN/VOXEL/SYNTH/PHYSICS/PLATFORM/GPU/GRAPHICS/AUDIO` come from the
module targets). Guides: `docs/FIRST_GAME.md` (short; keep it in sync with `templates/`) and `docs/GUIDE.md` (full code‑only guide;
its walkthrough is `examples/guide_game`, built in CI – keep them in sync).

## Editor
`.vscode/`: tasks (Build dev = Ctrl+Shift+B, Run game, Test, ASan, Lint, New game) and gdb launch configs
(Debug sandbox, offscreen, core/game tests with a doctest filter). All paths assume the `dev` preset (`build/`).

## Games
Declare every program with `mge_add_game(<name> SOURCES … [LIBRARIES …] [MODULES …] [HEADLESS] [SMOKE_FRAMES n])`
(`cmake/MgeGame.cmake`): it links main + modules and adds `run-<name>` and a smoke test. New game: `mge new <name> [--template 3d|2d|headless]`
(or `scripts/new-game.sh` wrapper) → `examples/<name>/` or standalone from `templates/` (`*.in`, CI builds and tests templates).

## Procgen
`mge_procgen` (headless, always built): `MeshData` (SoA, all attributes same length, `validate()`, `writeObj`).
GPU upload: `device.uploadMesh(meshData)` (mge_gpu). Visual checks: `examples/procgen_gallery` → `<build>/gallery/*.obj`.

## Voxel
`mge_voxel` (headless, always built): `BlockRegistry` (id 0 = air; `BlockDesc` colour and/or atlas tiles), 16³ chunks stacked in
all axes (`ChunkCoord`, `chunkOf`/`localOf`/`blockAt`; block (x,y,z) occupies [x,x+1)), `VoxelWorld` (+ `TerrainGenerator`,
dirty flags; loading dirties neighbours), `buildChunkMesh` → `{opaque, transparent}` MeshData in chunk‑local space (greedy for colour‑only
cubes, optional AO). Generators must be stateless/order‑independent (sandbox trees replay neighbour columns). The game only registers blocks and
implements a generator.

## GPU (SDL3 GPU)
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
Compute (#58): `.comp` shaders, `ComputePipelineDesc` → `device.createComputePipeline`, `cmd.beginComputePass(...)` → RAII `ComputePass`
(`bindPipeline/bindStorageBuffers/pushUniform/dispatch`), bindings set0 read‑only / set1 read‑write / set2 UBO; `downloadBuffer`.
`GpuNoiseField` = simplex/fBm density field on the GPU, matches `mge::Noise` within 1e‑4 (CPU stays the reference in tests).
Shader includes (#36): `#extension GL_GOOGLE_include_directive : require` + `#include "mge/lighting.glsl"` (also `sdf.glsl` = GLSL
mirror of `Sdf.h`, `raymarch.glsl`, `sdf_pass.glsl`); root `src/mge/graphics/shaders/include`, extra dirs via `INCLUDE_DIRS`;
rebuilt on include changes (depfile).

## Physics (M6, `mge_physics`, headless)
`<mge/voxel/VoxelRaycast.h>` `raycastVoxels(world, ray, maxDist[, pickable(BlockId)])` → `VoxelRaycastHit{block, normal, distance,
blockId, point, face}` (Amanatides–Woo DDA; ties prefer X>Y>Z; max 4096). `GridQuery.h`: `SolidQuery` = `bool(const glm::ivec3&)`,
`solidQuery(world)` (unloaded chunks = air). `VoxelCollision.h`: `moveAabb(box, delta, isSolid, {stepHeight, skin})` → `MoveResult{box,
moved, blocked, grounded, stepped}` – swept per axis (Y, X, Z), never tunnels; boxes already inside a block can move out.
`CharacterController.h`: ECS component (Walk/Fly, jump, gravity, step‑up; `Transform::position` = feet; `look(dYaw, dPitch)`; `input`
written by the game each frame; `enabled = false` while the chunk under the feet isn't loaded) + `addCharacterSystem(world, solid)`
(Fixed phase) + `CharacterCamera{target}` / `addCharacterCameraSystem(world, time)` (Render phase, interpolated with `alpha`).
`SpatialHash` (broadphase, sorted pairs), `RigidBody` (box/sphere, mass 0 = static, restitution, friction) + `PhysicsWorld::step`
(sorted by entity id → deterministic; corrections go through `moveAabb` so bodies never enter voxels) + `addPhysicsSystem`.
Sandbox controls: WASD, **Space** jump (fly: up/Shift down), **F** walk/fly, Ctrl sprint, **G** throw a box, **LMB** break / **RMB**
place (hotbar **1–9**, 6‑block reach, outline always shown), **T** fast time, **F2** LOD, F3/F4 debug, P pause, Esc quit.

## Profiling & performance (M7)
`app.profiler()` (`<mge/core/Profiler.h>`, main thread only): `MGE_PROFILE_SCOPE(profiler(), "name")`; phases (`frame`, `fixed`,
`update`, `render`, `modules.*`) and every World system are timed automatically; `DebugOverlay::setProfile(profiler)`. Off at runtime
with `MGE_PROFILE=0`, compiled out with `-DMGE_PROFILING=OFF`; optional Tracy `-DMGE_TRACY=ON` (scope names must be literals).
No GPU timestamps (SDL GPU has none). Benchmarks: `bench/mge_bench [--json f] [--filter s] [--quick]` (noise, ECS, voxel, streaming,
physics, profiler, GPU noise); CI compares with main and only warns; results < 1 ns/item are flagged "suspicious".
Size: `minsize` preset (MinSizeRel, LTO, gc‑sections, strip) + `scripts/check-size.sh build-minsize`. LOD (#61):
`StreamingSettings::lodRadius` rings (LOD 1/2 = 2×/4× downsampled meshes with skirts, water kept as flat quads) – `StreamMesh{coord,
mesh, lod}`; discarded/LOD‑changed meshes are re‑marked dirty; `setSettings`/`remeshAll` re‑mesh everything. Sandbox: F2, `MGE_LOD=0`.

## Sky and SDF rendering (M8)
`DayNight{hour, dayLengthSeconds}` (headless math): `advance(dt)`, `sky()` → `SkySettings`, `lighting(base)` (fog = horizon colour).
`SkyRenderer::draw(renderer, invViewProj, camPos, sky)` inside the MeshRenderer pass **before** opaque (fullscreen, depth ALWAYS, no
write). `SdfRenderer(device, color, depth, gameFrag)` – the game's fragment shader defines `map(p)` + `material(p)` and includes
`mge/sdf_pass.glsl`; `draw(renderer, SdfFrame{viewProj, invViewProj, camPos, time, params[4]}, lighting)` after opaque: raymarches,
lights like meshes and writes `gl_FragDepth`, so SDFs intersect rasterized geometry. CPU collision = same formulas in C++ (`Sdf.h`).

## Assets (ADR 0004, #65, `docs/ASSETS.md`)
Loaders (`Texture2D::fromFile`, `AudioModule::loadWav`, `loadTtfFontFile`, `platformer::loadLevel`) take paths **relative to the asset
root** (or absolute) and resolve them with `mge::assetPath` themselves – don't wrap them in `assetPath(...)`, never `"../assets/..."`.
The root is resolved lazily and silently (override: `MGE_ASSET_DIR`; `MGE_SOURCE_ASSET_FALLBACK=OFF` disables the source‑tree
fallback, as the zero‑asset CI job does). Loaders live in modules, never in `mge_core`; failures are logged and return an invalid value.

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
- `assets/levels` + `assets/sounds` are kept as 2D test data: `examples/platformer2d` (#45) loads them. Don't delete them.
- Size budgets: a change that legitimately grows a binary must update `cmake/size-budget.txt` (CI measured size + 10 %, rounded up
  to 4 KiB – take the number from the CI `size` job log, it links SDL3 statically).
- Stacked branches: merge PRs with merge commits (no squash/rebase), oldest first; update a branch with `git merge origin/main`.
