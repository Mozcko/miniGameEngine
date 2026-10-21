# Roadmap

Goal: a **heavily optimizable C++ engine** whose **core runs headless** (no window/GPU), with rendering, input and audio as optional modules. Games can be built from **pure math and code** (procedural meshes and shaders, SDF raymarching, synthesized audio, embedded font; even a single executable with no files) **or** use regular assets. Both paths are supported and can be mixed.

## Key decisions

Recorded as ADRs in [docs/adr/](adr/README.md). Contribution rules are in [CONTRIBUTING.md](../CONTRIBUTING.md).

| Decision | Choice |
|---|---|
| Headless ([ADR 0003](adr/0003-headless-core.md)) | `mge_core` builds/runs without SDL or a GPU; platform/render/audio are optional modules |
| Visuals | Hybrid: CPU procedural meshes + procedural shaders + optional SDF raymarching |
| Assets ([ADR 0004](adr/0004-procedural-first-assets-optional.md), supersedes 0002) | **Procedural‑first, assets optional**: every system has a code/math path; loading textures, fonts, sounds… is supported; a zero‑asset single‑binary build stays possible |
| GPU API ([ADR 0001](adr/0001-sdl3-gpu-api.md)) | **SDL3 GPU API** (Vulkan on Linux/Windows, Metal on macOS, D3D12 on Windows/Xbox). **No new work on OpenGL**; the GL renderer is removed after parity (#40) |
| Platform | **SDL3** (migration in #9) |

## Migration path OpenGL → SDL3 GPU

1. M1: port window, events, input and timer to SDL3 (#9). The legacy GL renderer keeps running through `SDL_GL_*`.
2. M3: SDL GPU wrappers (#28), then a build-time shader toolchain that embeds the bytecode (#29), then parity with the current sandbox (#30).
3. Delete OpenGL and GLEW (#40). Offscreen render tests run on lavapipe in CI (#39).

## Labels

- **Type:** `bug`, `enhancement`, `refactor`, `chore`, `performance`, `testing`, `documentation`, `epic`
- **Area:** `area:build|core|platform|procgen|voxel|render|ui|audio|physics|game|tooling`
- **Priority:** `priority:critical|high|medium|low`
- **Size:** `size:S` (≤1 day), `size:M` (a few days), `size:L` (a week or more)

## Milestones

### M0 · Foundation — [epic #68](https://github.com/Mozcko/miniGameEngine/issues/68)

_Make the repo safe to change: clean history, strict build, tests and CI._ Repo hygiene, hardened CMake, unit tests and CI so every later change has a safety net. Quick fixes for verified sandbox bugs.

| # | Issue | Labels |
|---|---|---|
| [#1](https://github.com/Mozcko/miniGameEngine/issues/1) | ✅ Commit documentation and stop tracking the build/ directory | chore, priority:critical, size:S |
| [#2](https://github.com/Mozcko/miniGameEngine/issues/2) | ✅ Harden CMake: Release default, warnings, C++17 required, compile_commands | chore, priority:high, size:S |
| [#3](https://github.com/Mozcko/miniGameEngine/issues/3) | ✅ Add a unit test framework (doctest + CTest) | testing, priority:high, size:S |
| [#4](https://github.com/Mozcko/miniGameEngine/issues/4) | ✅ GitHub Actions CI: Linux build with warnings + tests | chore, priority:high, size:M |
| [#5](https://github.com/Mozcko/miniGameEngine/issues/5) | ✅ Bug: trees and roses never spawn (RNG reuse in Chunk::generate) | bug, priority:high, size:S, good first issue |
| [#6](https://github.com/Mozcko/miniGameEngine/issues/6) | ✅ Bug: FPS counter divides by zero while paused | bug, priority:low, size:S, good first issue |
| [#7](https://github.com/Mozcko/miniGameEngine/issues/7) | ✅ Add ROADMAP.md, CONTRIBUTING.md and architecture decision records | documentation, priority:medium, size:S |

### M1 · Headless Core & SDL3 — [epic #69](https://github.com/Mozcko/miniGameEngine/issues/69)

_A core that builds and runs without SDL or a GPU, with SDL3 as the platform layer._ Engine split into libraries; mge_core builds and runs with no window/GPU. Platform layer on SDL3. Fixed-timestep loop, logging, deterministic RNG, input actions, ECS.

| # | Issue | Labels |
|---|---|---|
| [#8](https://github.com/Mozcko/miniGameEngine/issues/8) | ✅ Split the engine into library targets (mge_core, mge_platform, mge_render, mge_audio) | refactor, priority:critical, size:L |
| [#9](https://github.com/Mozcko/miniGameEngine/issues/9) | ✅ Migrate the platform layer from SDL2 to SDL3 | refactor, priority:critical, size:M |
| [#10](https://github.com/Mozcko/miniGameEngine/issues/10) | ✅ Unify namespaces and naming conventions | refactor, priority:high, size:L |
| [#11](https://github.com/Mozcko/miniGameEngine/issues/11) | ✅ Headless application runner with ApplicationConfig and optional modules | enhancement, priority:critical, size:L |
| [#12](https://github.com/Mozcko/miniGameEngine/issues/12) | ✅ Single clock and fixed-timestep game loop | enhancement, priority:high, size:M |
| [#13](https://github.com/Mozcko/miniGameEngine/issues/13) | ✅ Logging and assertion system | enhancement, priority:medium, size:S |
| [#14](https://github.com/Mozcko/miniGameEngine/issues/14) | ✅ Deterministic RNG and coordinate hashing | enhancement, priority:high, size:S |
| [#15](https://github.com/Mozcko/miniGameEngine/issues/15) | ✅ Backend-agnostic input with actions and pressed/released state | enhancement, priority:high, size:M |
| [#16](https://github.com/Mozcko/miniGameEngine/issues/16) | ✅ Lightweight ECS / scene system | enhancement, priority:high, size:L |
| [#17](https://github.com/Mozcko/miniGameEngine/issues/17) | ✅ Safe platform init/shutdown order | bug, priority:medium, size:S |
| [#18](https://github.com/Mozcko/miniGameEngine/issues/18) | ✅ Headless smoke test in CI | testing, priority:medium, size:S |

### M1.5 · Game Dev Experience — [epic #101](https://github.com/Mozcko/miniGameEngine/issues/101)

_Make building and iterating on a game easy, before M2 adds lots of new engine features._ One `game/` folder, run from anywhere, one‑call game targets + a template, VS Code build/run/debug, an umbrella header and a first‑game guide.

| # | Issue | Labels |
|---|---|---|
| [#96](https://github.com/Mozcko/miniGameEngine/issues/96) | ✅ Rename games/ to game/ (single game folder) | refactor, priority:high, size:S |
| [#97](https://github.com/Mozcko/miniGameEngine/issues/97) | ✅ Run the game from anywhere (assets relative to the executable) + run target | enhancement, priority:high, size:S |
| [#98](https://github.com/Mozcko/miniGameEngine/issues/98) | ✅ mge_add_game() CMake function + new-game template | enhancement, priority:high, size:M |
| [#99](https://github.com/Mozcko/miniGameEngine/issues/99) | ✅ VS Code setup: build, run, debug and test from the editor | chore, priority:medium, size:S |
| [#100](https://github.com/Mozcko/miniGameEngine/issues/100) | ✅ Umbrella header <mge/mge.h> + 'your first game' guide | documentation, priority:medium, size:S |

### M2 · Procedural Geometry & Generation — [epic #70](https://github.com/Mozcko/miniGameEngine/issues/70)

_Generate geometry and worlds from math, fully testable headless._ CPU-side, headless-testable generation: MeshData, primitives, mesh ops, noise, SDF + isosurfaces, geometry math, engine voxel module.

| # | Issue | Labels |
|---|---|---|
| [#19](https://github.com/Mozcko/miniGameEngine/issues/19) | ✅ CPU MeshData independent of the GPU | enhancement, priority:high, size:M |
| [#20](https://github.com/Mozcko/miniGameEngine/issues/20) | ✅ Procedural primitive generators | enhancement, priority:high, size:M |
| [#21](https://github.com/Mozcko/miniGameEngine/issues/21) | ✅ Mesh operations: transform, merge, normals, extrude, lathe, sweep | enhancement, priority:medium, size:M |
| [#22](https://github.com/Mozcko/miniGameEngine/issues/22) | ✅ Noise library | enhancement, priority:high, size:M |
| [#23](https://github.com/Mozcko/miniGameEngine/issues/23) | ✅ SDF library and isosurface extraction | enhancement, priority:medium, size:L |
| [#24](https://github.com/Mozcko/miniGameEngine/issues/24) | ✅ Geometry math: AABB, sphere, plane, frustum, ray tests | enhancement, priority:high, size:M |
| [#25](https://github.com/Mozcko/miniGameEngine/issues/25) | ✅ Move the voxel world into an engine module (mge_voxel) | refactor, priority:high, size:L |
| [#26](https://github.com/Mozcko/miniGameEngine/issues/26) | ✅ Neighbour-aware and greedy voxel meshing | enhancement, priority:high, size:M |
| [#27](https://github.com/Mozcko/miniGameEngine/issues/27) | ✅ Noise-based terrain and biome generator | enhancement, priority:medium, size:M |

### M3 · GPU Renderer (SDL3 GPU) — [epic #71](https://github.com/Mozcko/miniGameEngine/issues/71)

_Move rendering to SDL3 GPU (Vulkan/Metal/D3D12) with embedded shaders and procedural visuals; delete OpenGL._ Rendering on the SDL3 GPU API (Vulkan/Metal/D3D12) with embedded shader bytecode; parity with the old OpenGL sandbox, then procedural materials/lighting/sky; OpenGL removed.

| # | Issue | Labels |
|---|---|---|
| [#28](https://github.com/Mozcko/miniGameEngine/issues/28) | ✅ Thin RAII wrappers over the SDL3 GPU API | enhancement, priority:critical, size:M |
| [#29](https://github.com/Mozcko/miniGameEngine/issues/29) | ✅ Shader toolchain: build-time compilation to embedded bytecode | chore, priority:critical, size:M |
| [#30](https://github.com/Mozcko/miniGameEngine/issues/30) | ✅ Port mesh and chunk rendering to SDL GPU (parity with the OpenGL sandbox) | enhancement, priority:critical, size:L |
| [#31](https://github.com/Mozcko/miniGameEngine/issues/31) | ✅ Render queue: opaque, cutout and transparent passes | enhancement, priority:high, size:M |
| [#32](https://github.com/Mozcko/miniGameEngine/issues/32) | ✅ Camera rework: components, real frustum culling, pixel ortho, resize | enhancement, priority:high, size:M |
| [#33](https://github.com/Mozcko/miniGameEngine/issues/33) | ✅ Lighting model: directional, hemisphere ambient, fog | enhancement, priority:medium, size:M |
| [#34](https://github.com/Mozcko/miniGameEngine/issues/34) | ✅ Procedural materials alongside textures | enhancement, priority:high, size:L |
| [#35](https://github.com/Mozcko/miniGameEngine/issues/35) | ↪ moved to M8 · Procedural sky | enhancement, priority:low, size:M |
| [#36](https://github.com/Mozcko/miniGameEngine/issues/36) | ↪ moved to M8 · SDF raymarching render pass | enhancement, priority:low, size:L |
| [#37](https://github.com/Mozcko/miniGameEngine/issues/37) | ✅ Swapchain and device configuration | chore, priority:medium, size:S |
| [#38](https://github.com/Mozcko/miniGameEngine/issues/38) | ✅ Debug draw utilities | enhancement, priority:medium, size:S |
| [#39](https://github.com/Mozcko/miniGameEngine/issues/39) | ✅ Offscreen render tests in CI (lavapipe) | testing, priority:medium, size:M |
| [#40](https://github.com/Mozcko/miniGameEngine/issues/40) | ✅ Remove the legacy OpenGL / GLEW renderer | chore, priority:high, size:S |

### M4 · Procedural UI & Text — [epic #72](https://github.com/Mozcko/miniGameEngine/issues/72)

_UI and text without font files._ Embedded bitmap font, immediate-mode pixel-space 2D batch renderer, debug overlay. SDL_ttf and legacy 2D classes removed.

| # | Issue | Labels |
|---|---|---|
| [#41](https://github.com/Mozcko/miniGameEngine/issues/41) | ✅ Embedded bitmap font (default) + optional TTF loading | enhancement, priority:high, size:M |
| [#42](https://github.com/Mozcko/miniGameEngine/issues/42) | ✅ Immediate-mode 2D batch renderer in pixel space | enhancement, priority:high, size:M |
| [#43](https://github.com/Mozcko/miniGameEngine/issues/43) | ✅ Debug overlay: FPS, frame-time graph, counters | enhancement, priority:medium, size:S |
| [#44](https://github.com/Mozcko/miniGameEngine/issues/44) | ✅ Bug: HUD labels invisible, upside down and wrongly colored | bug, priority:medium, size:S |
| [#45](https://github.com/Mozcko/miniGameEngine/issues/45) | ✅ Remove legacy Sprite and Animation (2D levels/sounds kept as test data: `examples/platformer2d`) | refactor, priority:low, size:S |

### M5 · Procedural Audio — [epic #73](https://github.com/Mozcko/miniGameEngine/issues/73)

_All sound generated by code._ SDL3 audio streams, synthesizer, parametric SFX and a code-defined music sequencer. SDL_mixer removed.

| # | Issue | Labels |
|---|---|---|
| [#46](https://github.com/Mozcko/miniGameEngine/issues/46) | ✅ Audio output via SDL3 audio streams (synthesis + sample playback) | enhancement, priority:high, size:M |
| [#47](https://github.com/Mozcko/miniGameEngine/issues/47) | ✅ Synthesizer core | enhancement, priority:high, size:L |
| [#48](https://github.com/Mozcko/miniGameEngine/issues/48) | ✅ Parametric sound-effect generator (sfxr-style) | enhancement, priority:medium, size:M |
| [#49](https://github.com/Mozcko/miniGameEngine/issues/49) | ✅ Code-defined music sequencer | enhancement, priority:low, size:L |
| [#50](https://github.com/Mozcko/miniGameEngine/issues/50) | ✅ Headless offline audio rendering for tests | testing, priority:low, size:S |

### M6 · Physics & Gameplay — [epic #74](https://github.com/Mozcko/miniGameEngine/issues/74)

_Robust collision and a proper character controller._ Voxel raycasts, swept AABB collision, character controller decoupled from the camera, basic rigid bodies.

| # | Issue | Labels |
|---|---|---|
| [#51](https://github.com/Mozcko/miniGameEngine/issues/51) | Voxel DDA raycast | enhancement, priority:high, size:S |
| [#52](https://github.com/Mozcko/miniGameEngine/issues/52) | Swept AABB collision against the voxel world | enhancement, priority:high, size:M |
| [#53](https://github.com/Mozcko/miniGameEngine/issues/53) | Character controller decoupled from the camera | refactor, priority:high, size:M |
| [#54](https://github.com/Mozcko/miniGameEngine/issues/54) | Basic rigid bodies and spatial-hash broadphase | enhancement, priority:low, size:L |

### M7 · Performance & Size — [epic #75](https://github.com/Mozcko/miniGameEngine/issues/75)

_Measure, parallelise, stream and shrink._ Profiling, job system, chunk streaming, GPU compute generation, CI benchmarks and a binary-size budget.

| # | Issue | Labels |
|---|---|---|
| [#55](https://github.com/Mozcko/miniGameEngine/issues/55) | Profiling instrumentation | performance, priority:high, size:S |
| [#56](https://github.com/Mozcko/miniGameEngine/issues/56) | ✅ (done early, with M3) Job system / thread pool | performance, priority:high, size:L |
| [#57](https://github.com/Mozcko/miniGameEngine/issues/57) | ✅ (done early, with M3) Chunk streaming with unload and memory budget | performance, priority:high, size:M |
| [#58](https://github.com/Mozcko/miniGameEngine/issues/58) | GPU compute procedural generation | performance, priority:low, size:L |
| [#59](https://github.com/Mozcko/miniGameEngine/issues/59) | Headless benchmarks in CI | performance, testing, priority:medium, size:M |
| [#60](https://github.com/Mozcko/miniGameEngine/issues/60) | Binary size budget | performance, priority:medium, size:S |
| [#61](https://github.com/Mozcko/miniGameEngine/issues/61) | Terrain level of detail | performance, priority:low, size:L |

### M8 · Showcase — [epic #76](https://github.com/Mozcko/miniGameEngine/issues/76)

_A showcase game that demonstrates the engine: procedural‑first, using assets only where they make sense (ADR 0004)._ Asset loaders stay behind clean APIs; a zero‑asset build stays possible.

| # | Issue | Labels |
|---|---|---|
| [#62](https://github.com/Mozcko/miniGameEngine/issues/62) | Rebuild the sandbox on the new engine (procedural-first showcase) | enhancement, priority:high, size:L |
| [#63](https://github.com/Mozcko/miniGameEngine/issues/63) | Block break and place with selection outline | enhancement, priority:medium, size:M |
| [#64](https://github.com/Mozcko/miniGameEngine/issues/64) | Synthesized sound effects and music in the showcase | enhancement, priority:medium, size:S |
| [#65](https://github.com/Mozcko/miniGameEngine/issues/65) | Optional asset loading behind clean APIs + zero-asset build check | chore, priority:high, size:S |
| [#66](https://github.com/Mozcko/miniGameEngine/issues/66) | Second demo: tiny SDF-based game | enhancement, priority:low, size:M |
| [#67](https://github.com/Mozcko/miniGameEngine/issues/67) | Engine user guide: build a game with code only | documentation, priority:medium, size:M |

### M9 · mge CLI & Project Wizard (post‑1.0) — [epic #107](https://github.com/Mozcko/miniGameEngine/issues/107)

**Scheduled after the MVP/1.0** (owner's decision: developer‑experience and engine extras wait until the engine core is complete).

_A cargo‑like workflow: `mge new` (wizard) → `mge run`, with the engine consumed as a normal dependency._ #103 and #104 only need M1.5 and can start early. The 2D/3D templates need M3 (renderer) and M4 (2D batch + font); the headless template can come first.

| # | Issue | Labels |
|---|---|---|
| [#103](https://github.com/Mozcko/miniGameEngine/issues/103) | Engine as a reusable dependency (installable CMake package + FetchContent) | enhancement, priority:high, size:M |
| [#104](https://github.com/Mozcko/miniGameEngine/issues/104) | mge CLI: cargo-like new/build/run/test/check/doctor | enhancement, priority:high, size:L |
| [#105](https://github.com/Mozcko/miniGameEngine/issues/105) | Interactive project wizard (TUI) for mge new | enhancement, priority:medium, size:M |
| [#106](https://github.com/Mozcko/miniGameEngine/issues/106) | Project templates: 2D, 3D and headless | enhancement, priority:medium, size:L |

## Suggested order

✅ M0 (#1–#7), ✅ M1 (#8–#18), ✅ M1.5 (#96–#100), ✅ M2 (#19–#27), ✅ M3 (#28–#34, #37–#40; #35/#36 moved to M8), ✅ M4 (#41–#45) and ✅ M5 (#46–#50) are done, plus #56/#57 from M7 (job system, chunk streaming). Next: M6 (physics and gameplay). Each issue's **Depends on** section on GitHub lists its prerequisites.
