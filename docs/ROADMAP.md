# Roadmap

Goal: a **heavily optimizable C++ engine** whose **core runs headless** (no window/GPU), with rendering, input and audio as optional modules. Games can be built from **pure math and code** (procedural meshes and shaders, SDF raymarching, synthesized audio, embedded font; even a single executable with no files) **or** use regular assets. Both paths are supported and can be mixed.

**Current release: 1.0.2.** M0–M10 and the 1.0.x polish are done. The engine is great today for voxel, procedural/SDF and
simple 2D games written in C++, C or Python. **Next:** make it able to ship full asset-based 2D and 3D games (M11), with the
codebase in one language, and then bring it to more platforms and languages (M12).

## Key decisions

Recorded as ADRs in [docs/adr/](adr/README.md). Contribution rules are in [CONTRIBUTING.md](../CONTRIBUTING.md).

| Decision | Choice |
|---|---|
| Headless ([ADR 0003](adr/0003-headless-core.md)) | `mge_core` builds/runs without SDL or a GPU; platform/render/audio are optional modules |
| Visuals | Hybrid: CPU procedural meshes + procedural shaders + optional SDF raymarching |
| Assets ([ADR 0004](adr/0004-procedural-first-assets-optional.md), supersedes 0002) | **Procedural‑first, assets optional**: every system has a code/math path; loading textures, fonts, sounds, models… is supported; a zero‑asset single‑binary build stays possible |
| GPU API ([ADR 0001](adr/0001-sdl3-gpu-api.md)) | **SDL3 GPU API** (Vulkan on Linux/Windows, Metal on macOS, D3D12 on Windows/Xbox). No OpenGL |
| Platform | **SDL3** |
| Languages ([ADR 0006](adr/0006-language-bindings.md)) | The C API is the stable ABI for other languages; Python binds the C++ API through nanobind |
| Project language ([#236](https://github.com/Mozcko/miniGameEngine/issues/236)) | **English** for code comments, log messages, test names and docs |
| Priorities | **Game-ready features (M11) before new platforms and languages (M12)**, so new targets ship an engine that can make full games |

## Labels

- **Type:** `bug`, `enhancement`, `refactor`, `chore`, `performance`, `testing`, `documentation`, `epic`
- **Area:** `area:build|core|platform|procgen|voxel|render|ui|audio|physics|game|tooling`
- **Priority:** `priority:critical|high|medium|low`
- **Size:** `size:S` (≤1 day), `size:M` (a few days), `size:L` (a week or more)

## Release plan

| Release | Contents | Status |
|---|---|---|
| 0.9.0 | M0–M9: headless core, SDL3 GPU renderer, procedural generation, voxels, physics, audio, UI/text, performance, showcase, package + CLI | ✅ |
| 1.0.0 | M10: C API and Python bindings | ✅ |
| 1.0.1 | M10 review fixes (bindings memory safety) | ✅ |
| 1.0.2 | 1.0.x polish epic [#189](https://github.com/Mozcko/miniGameEngine/issues/189) | ✅ |
| 1.1.0 | M11 phase 1: third-person 3D action features + the KayKit vertical slice | planned |
| 1.2.0 | M11 phase 2: 2D toolkit, dialogue, asset manager + the 2D vertical slice | planned |
| 1.3.0 | M11 phase 3: post-processing, full rigid-body physics, large worlds, networking | planned |
| 1.4.0+ | M12: Windows CLI, macOS/Metal, PyPI wheels, more languages | planned |

Bug-fix releases (1.x.y) go out whenever fixes land after a tag. Every release needs a CHANGELOG entry and a pushed `vX.Y.Z`
tag on the release PR's merge commit.

## Milestones

### 1.0.x · Polish — [epic #189](https://github.com/Mozcko/miniGameEngine/issues/189)

**Done — released as 1.0.1 and 1.0.2.** Bindings safety fixes, stable ECS pointers, better rigid-body contacts, correct LOD
terrain and water, per-face transparency sorting.

| # | Issue | Labels |
|---|---|---|
| [#175](https://github.com/Mozcko/miniGameEngine/issues/175) | ✅ Release 1.0.1 with the M10 review fixes | chore, priority:high, size:S |
| [#176](https://github.com/Mozcko/miniGameEngine/issues/176) | ✅ Stable storage for runtime ECS components (pointers survive adds) | refactor, priority:medium, size:M |
| [#177](https://github.com/Mozcko/miniGameEngine/issues/177) | ✅ sdf_glide: accumulate mouse steering between fixed steps | bug, good first issue, size:S |
| [#178](https://github.com/Mozcko/miniGameEngine/issues/178) | ✅ LOD: distant shallow water renders as ground | bug, priority:low, size:S |
| [#179](https://github.com/Mozcko/miniGameEngine/issues/179) | ✅ LOD: distant terrain looks up to f−1 blocks taller | enhancement, priority:low, size:M |
| [#180](https://github.com/Mozcko/miniGameEngine/issues/180) | ✅ Transparent pass: sort faces inside a chunk | bug, priority:low, size:M |
| [#181](https://github.com/Mozcko/miniGameEngine/issues/181) | ✅ Rigid bodies: resting-contact friction and true sphere vs voxel collision | enhancement, priority:low, size:M |

### M11.0 · English everywhere — [issue #236](https://github.com/Mozcko/miniGameEngine/issues/236)

_One language for the whole codebase._ First a small PR changes the convention (`CONTRIBUTING.md`, `CLAUDE.md`, PR template),
then the existing Spanish comments, log messages and test names are translated area by area. Every sub-issue is a good first
issue. Can run in parallel with M11; new code is written in English from the convention change on.

| # | Issue | Labels |
|---|---|---|
| [#237](https://github.com/Mozcko/miniGameEngine/issues/237) | Translate to English: `src/mge/core` | chore, good first issue, size:S |
| [#238](https://github.com/Mozcko/miniGameEngine/issues/238) | Translate to English: `procgen`, `synth`, `audio` | chore, good first issue, size:S |
| [#239](https://github.com/Mozcko/miniGameEngine/issues/239) | Translate to English: `voxel`, `physics` | chore, good first issue, size:S |
| [#240](https://github.com/Mozcko/miniGameEngine/issues/240) | Translate to English: `gpu`, `graphics` (+ shaders), `platform`, `app`, `c`, Python bindings | chore, good first issue, size:S |
| [#241](https://github.com/Mozcko/miniGameEngine/issues/241) | Translate to English: `game/` and `examples/` | chore, good first issue, size:S |
| [#242](https://github.com/Mozcko/miniGameEngine/issues/242) | Translate to English: `tools/mge` and `templates/` | chore, good first issue, size:S |
| [#243](https://github.com/Mozcko/miniGameEngine/issues/243) | Translate to English: `tests/` (test names and comments) | chore, testing, good first issue, size:M |
| [#244](https://github.com/Mozcko/miniGameEngine/issues/244) | Translate to English: CMake files, scripts and bench | chore, good first issue, size:S |

### M11 · Game-ready features — [epic #199](https://github.com/Mozcko/miniGameEngine/issues/199)

_Everything an asset-based 2D or 3D game needs, from platformers to a third-person soulslike._ Headless first: parsing,
animation sampling, collision, navigation, serialization, UI layout and AI live in headless libraries with tests, and only
upload/draw touches the GPU. Asset formats (glTF, Tiled, OGG) are optional loaders in modules. Public game APIs also land
in the C API and Python bindings. Done when both vertical slices ([#225](https://github.com/Mozcko/miniGameEngine/issues/225), [#226](https://github.com/Mozcko/miniGameEngine/issues/226)) build and run in CI. The epic has a table
of the 30 genres this unlocks and what each one needs.

#### Phase 1 · 3D third-person action (→ 1.1.0)

| # | Issue | Labels |
|---|---|---|
| [#206](https://github.com/Mozcko/miniGameEngine/issues/206) | Gamepad input, rebinding and text input | enhancement, priority:high, size:M |
| [#200](https://github.com/Mozcko/miniGameEngine/issues/200) | glTF 2.0 model import (meshes, materials, node hierarchy) | enhancement, priority:high, size:L |
| [#201](https://github.com/Mozcko/miniGameEngine/issues/201) | Skeletal animation and GPU skinning | enhancement, priority:high, size:L |
| [#202](https://github.com/Mozcko/miniGameEngine/issues/202) | Animator: crossfades, state machine, animation events, root motion | enhancement, priority:high, size:M |
| [#203](https://github.com/Mozcko/miniGameEngine/issues/203) | Collision shapes and queries for non-voxel worlds (meshes, capsules, triggers, layers) | enhancement, priority:high, size:L |
| [#204](https://github.com/Mozcko/miniGameEngine/issues/204) | Capsule character controller for mesh worlds (slopes, steps, ground snapping) | enhancement, priority:high, size:M |
| [#205](https://github.com/Mozcko/miniGameEngine/issues/205) | Third-person camera rig: orbit, collision, target lock-on | enhancement, priority:high, size:S |
| [#207](https://github.com/Mozcko/miniGameEngine/issues/207) | UI toolkit: widgets, layout, focus navigation, menus, HUD | enhancement, priority:high, size:L |
| [#208](https://github.com/Mozcko/miniGameEngine/issues/208) | Scene/prefab serialization and save games | enhancement, priority:high, size:L |
| [#209](https://github.com/Mozcko/miniGameEngine/issues/209) | Navigation and pathfinding: grid/hex A*, flow fields, navmesh, steering | enhancement, priority:high, size:L |
| [#210](https://github.com/Mozcko/miniGameEngine/issues/210) | Gameplay AI toolkit: state machines, behaviour trees, perception | enhancement, priority:medium, size:M |
| [#211](https://github.com/Mozcko/miniGameEngine/issues/211) | Shadows: directional shadow maps (cascaded) | enhancement, priority:medium, size:L |
| [#212](https://github.com/Mozcko/miniGameEngine/issues/212) | Point and spot lights | enhancement, priority:medium, size:M |
| [#213](https://github.com/Mozcko/miniGameEngine/issues/213) | Particle system (2D and 3D) | enhancement, priority:medium, size:M |
| [#218](https://github.com/Mozcko/miniGameEngine/issues/218) | 3D positional audio and OGG music streaming | enhancement, priority:medium, size:M |
| [#225](https://github.com/Mozcko/miniGameEngine/issues/225) | **Vertical slice:** third-person action RPG sample with KayKit assets | enhancement, priority:high, size:L |

#### Phase 2 · 2D and management games (→ 1.2.0)

| # | Issue | Labels |
|---|---|---|
| [#216](https://github.com/Mozcko/miniGameEngine/issues/216) | 2D toolkit: sprites, sprite-sheet animation, 2D camera, tilemaps (Tiled import) | enhancement, priority:high, size:M |
| [#217](https://github.com/Mozcko/miniGameEngine/issues/217) | 2D collision and a kinematic platformer controller in the engine | enhancement, priority:medium, size:M |
| [#220](https://github.com/Mozcko/miniGameEngine/issues/220) | Gameplay utilities: tweens, timers, coroutines, event bus | enhancement, priority:low, size:S |
| [#221](https://github.com/Mozcko/miniGameEngine/issues/221) | Dialogue runtime and localization | enhancement, priority:medium, size:M |
| [#214](https://github.com/Mozcko/miniGameEngine/issues/214) | GPU instancing for repeated meshes and sprites | enhancement, performance, priority:medium, size:M |
| [#219](https://github.com/Mozcko/miniGameEngine/issues/219) | Asset manager: handles, caching, async loading, hot reload, release packaging | enhancement, priority:medium, size:M |
| [#226](https://github.com/Mozcko/miniGameEngine/issues/226) | **Vertical slice:** 2D action-platformer sample (metroidvania room set) | enhancement, priority:medium, size:M |

#### Phase 3 · Heavier genres (→ 1.3.0)

| # | Issue | Labels |
|---|---|---|
| [#215](https://github.com/Mozcko/miniGameEngine/issues/215) | Post-processing chain (bloom, tonemapping, retro filters) | enhancement, priority:low, size:M |
| [#222](https://github.com/Mozcko/miniGameEngine/issues/222) | Rotational rigid bodies and joints (evaluate Jolt as an optional module) | enhancement, priority:low, size:L |
| [#224](https://github.com/Mozcko/miniGameEngine/issues/224) | Large worlds: floating origin and streaming of non-voxel scenes | enhancement, priority:low, size:M |
| [#223](https://github.com/Mozcko/miniGameEngine/issues/223) | Networking: transport, replication, lobbies (rollback as an option) | enhancement, priority:low, size:L |

#### Good first issues

Small, self-contained pieces of M11 for new contributors (most build headless, without SDL or a GPU):

| # | Issue | Feeds |
|---|---|---|
| [#227](https://github.com/Mozcko/miniGameEngine/issues/227) | Easing functions (`mge::ease`) | #220 |
| [#228](https://github.com/Mozcko/miniGameEngine/issues/228) | Timer and Cooldown helpers | #220 |
| [#229](https://github.com/Mozcko/miniGameEngine/issues/229) | Frame-rate independent smoothing (`damp`, `smoothDamp`) | #205, #216 |
| [#230](https://github.com/Mozcko/miniGameEngine/issues/230) | Stick deadzones and response curves | #206 |
| [#231](https://github.com/Mozcko/miniGameEngine/issues/231) | Grid A* pathfinding | #209 |
| [#232](https://github.com/Mozcko/miniGameEngine/issues/232) | Camera2D: transforms, zoom, follow with deadzone, screen shake | #216 |
| [#233](https://github.com/Mozcko/miniGameEngine/issues/233) | Batch2D: rotated, scaled and flipped sprites | #216 |
| [#234](https://github.com/Mozcko/miniGameEngine/issues/234) | Example: zero-asset Breakout using Renderer2D | #199 |
| [#235](https://github.com/Mozcko/miniGameEngine/issues/235) | Load OGG Vorbis sound effects | #218 |

### M12 · Platforms & more languages — [epic #190](https://github.com/Mozcko/miniGameEngine/issues/190)

_Windows and macOS as first-class targets, prebuilt Python wheels, and more languages on the C API._ Starts after M11's
vertical slices.

| # | Issue | Labels |
|---|---|---|
| [#182](https://github.com/Mozcko/miniGameEngine/issues/182) | mge CLI on Windows | enhancement, priority:medium, size:M |
| [#183](https://github.com/Mozcko/miniGameEngine/issues/183) | macOS support verified in CI (Metal via SDL3 GPU) | enhancement, priority:medium, size:M |
| [#184](https://github.com/Mozcko/miniGameEngine/issues/184) | Prebuilt Python wheels and PyPI publishing | enhancement, priority:medium, size:M |
| [#186](https://github.com/Mozcko/miniGameEngine/issues/186) | Zig and Odin examples on the C API | enhancement, priority:low, size:S |
| [#185](https://github.com/Mozcko/miniGameEngine/issues/185) | Rust bindings on the C API (mge-sys + safe wrapper) | enhancement, priority:low, size:L |
| [#187](https://github.com/Mozcko/miniGameEngine/issues/187) | C# bindings on the C API | enhancement, priority:low, size:L |
| [#188](https://github.com/Mozcko/miniGameEngine/issues/188) | Lua scripting for C++ games | enhancement, priority:low, size:M |

## Suggested order

1. **M11.0 step 1** (the English convention change, [#236](https://github.com/Mozcko/miniGameEngine/issues/236)); the translation sub-issues can then run in parallel with everything else.
2. **M11 phase 1**, roughly in the order of its table: gamepad and glTF first, then animation, collision and the controller,
   then UI, saves, navigation and AI, then the rendering/audio polish, and finally the KayKit vertical slice ([#225](https://github.com/Mozcko/miniGameEngine/issues/225)). The good first
   issues can be picked up at any time. Release **1.1.0**.
3. **M11 phase 2**, ending with the 2D vertical slice ([#226](https://github.com/Mozcko/miniGameEngine/issues/226)). Release **1.2.0**.
4. **M11 phase 3.** Release **1.3.0**.
5. **M12** ([#190](https://github.com/Mozcko/miniGameEngine/issues/190)): Windows CLI and macOS first (they unlock wheels for those platforms), then PyPI wheels, then Zig/Odin,
   Rust, C# and Lua.

Each issue's **Depends on** section on GitHub lists its prerequisites.

## Completed milestones (M0–M10)

✅ M0 (#1–#7), ✅ M1 (#8–#18), ✅ M1.5 (#96–#100), ✅ M2 (#19–#27), ✅ M3 (#28–#34, #37–#40), ✅ M4 (#41–#45), ✅ M5 (#46–#50),
✅ M6 (#51–#54), ✅ M7 (#55–#61), ✅ M8 (#35, #36, #62–#67) and ✅ M9 (#103–#106) were released as **0.9.0**; ✅ M10 (#160–#163)
completed **1.0.0**. The tables below are kept as the project's history.

<details>
<summary>History: the OpenGL → SDL3 GPU migration</summary>

1. M1: port window, events, input and timer to SDL3 (#9). The legacy GL renderer keeps running through `SDL_GL_*`.
2. M3: SDL GPU wrappers (#28), then a build-time shader toolchain that embeds the bytecode (#29), then parity with the current sandbox (#30).
3. Delete OpenGL and GLEW (#40). Offscreen render tests run on lavapipe in CI (#39).

</details>

<details>
<summary>History: milestones M0–M10 with every issue</summary>

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
| [#35](https://github.com/Mozcko/miniGameEngine/issues/35) | ✅ (done in M8) Procedural sky | enhancement, priority:low, size:M |
| [#36](https://github.com/Mozcko/miniGameEngine/issues/36) | ✅ (done in M8) SDF raymarching render pass | enhancement, priority:low, size:L |
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
| [#51](https://github.com/Mozcko/miniGameEngine/issues/51) | ✅ Voxel DDA raycast | enhancement, priority:high, size:S |
| [#52](https://github.com/Mozcko/miniGameEngine/issues/52) | ✅ Swept AABB collision against the voxel world | enhancement, priority:high, size:M |
| [#53](https://github.com/Mozcko/miniGameEngine/issues/53) | ✅ Character controller decoupled from the camera | refactor, priority:high, size:M |
| [#54](https://github.com/Mozcko/miniGameEngine/issues/54) | ✅ Basic rigid bodies and spatial-hash broadphase | enhancement, priority:low, size:L |

### M7 · Performance & Size — [epic #75](https://github.com/Mozcko/miniGameEngine/issues/75)

_Measure, parallelise, stream and shrink._ Profiling, job system, chunk streaming, GPU compute generation, CI benchmarks and a binary-size budget.

| # | Issue | Labels |
|---|---|---|
| [#55](https://github.com/Mozcko/miniGameEngine/issues/55) | ✅ Profiling instrumentation | performance, priority:high, size:S |
| [#56](https://github.com/Mozcko/miniGameEngine/issues/56) | ✅ (done early, with M3) Job system / thread pool | performance, priority:high, size:L |
| [#57](https://github.com/Mozcko/miniGameEngine/issues/57) | ✅ (done early, with M3) Chunk streaming with unload and memory budget | performance, priority:high, size:M |
| [#58](https://github.com/Mozcko/miniGameEngine/issues/58) | ✅ GPU compute procedural generation | performance, priority:low, size:L |
| [#59](https://github.com/Mozcko/miniGameEngine/issues/59) | ✅ Headless benchmarks in CI | performance, testing, priority:medium, size:M |
| [#60](https://github.com/Mozcko/miniGameEngine/issues/60) | ✅ Binary size budget | performance, priority:medium, size:S |
| [#61](https://github.com/Mozcko/miniGameEngine/issues/61) | ✅ Terrain level of detail | performance, priority:low, size:L |

### M8 · Showcase — [epic #76](https://github.com/Mozcko/miniGameEngine/issues/76)

_A showcase game that demonstrates the engine: procedural‑first, using assets only where they make sense (ADR 0004)._ Asset loaders stay behind clean APIs; a zero‑asset build stays possible.

| # | Issue | Labels |
|---|---|---|
| [#35](https://github.com/Mozcko/miniGameEngine/issues/35) | ✅ Procedural sky and day/night cycle (moved from M3) | enhancement, priority:low, size:M |
| [#36](https://github.com/Mozcko/miniGameEngine/issues/36) | ✅ SDF raymarching render pass + shader `#include` (moved from M3) | enhancement, priority:low, size:L |
| [#62](https://github.com/Mozcko/miniGameEngine/issues/62) | ✅ Rebuild the sandbox on the new engine (procedural-first showcase) | enhancement, priority:high, size:L |
| [#63](https://github.com/Mozcko/miniGameEngine/issues/63) | ✅ Block break and place with selection outline | enhancement, priority:medium, size:M |
| [#64](https://github.com/Mozcko/miniGameEngine/issues/64) | ✅ Synthesized sound effects and music in the showcase | enhancement, priority:medium, size:S |
| [#65](https://github.com/Mozcko/miniGameEngine/issues/65) | ✅ Optional asset loading behind clean APIs + zero-asset build check | chore, priority:high, size:S |
| [#66](https://github.com/Mozcko/miniGameEngine/issues/66) | ✅ Second demo: tiny SDF-based game | enhancement, priority:low, size:M |
| [#67](https://github.com/Mozcko/miniGameEngine/issues/67) | ✅ Engine user guide: build a game with code only | documentation, priority:medium, size:M |

### M9 · mge CLI & Project Wizard — [epic #107](https://github.com/Mozcko/miniGameEngine/issues/107)

**Done — released as 0.9.0.** Engine as a package (`find_package(mge)` / FetchContent), standalone 3D/2D/headless templates, the `mge` CLI and the `mge new` wizard.

_A cargo‑like workflow: `mge new` (wizard) → `mge run`, with the engine consumed as a normal dependency._ #103 and #104 only need M1.5 and can start early. The 2D/3D templates need M3 (renderer) and M4 (2D batch + font); the headless template can come first.

| # | Issue | Labels |
|---|---|---|
| [#103](https://github.com/Mozcko/miniGameEngine/issues/103) | ✅ Engine as a reusable dependency (installable CMake package + FetchContent) | enhancement, priority:high, size:M |
| [#104](https://github.com/Mozcko/miniGameEngine/issues/104) | ✅ mge CLI: cargo-like new/build/run/test/check/doctor | enhancement, priority:high, size:L |
| [#105](https://github.com/Mozcko/miniGameEngine/issues/105) | ✅ Interactive project wizard (TUI) for mge new | enhancement, priority:medium, size:M |
| [#106](https://github.com/Mozcko/miniGameEngine/issues/106) | ✅ Project templates: 2D, 3D and headless | enhancement, priority:medium, size:L |

### M10 · Language Bindings — [epic #164](https://github.com/Mozcko/miniGameEngine/issues/164)

_Write games in **C** and **Python**, not only C++._ A stable C API (`mge_c`) is the ABI every non‑C++ language binds to; Python uses nanobind over the C++ API with the same v1 scope. Starts simple with C and Python; more languages (Rust, Zig, Odin, C#, Lua…) can follow on the C API. Needs #103 from M9; #163 also needs #104–#106.

| # | Issue | Labels |
|---|---|---|
| [#160](https://github.com/Mozcko/miniGameEngine/issues/160) | ✅ [ADR 0006](adr/0006-language-bindings.md): language bindings strategy (C API as stable ABI, Python via nanobind) | documentation, priority:high, size:S |
| [#161](https://github.com/Mozcko/miniGameEngine/issues/161) | ✅ [C API v1 (`mge_c`)](C_API.md): write games in plain C | enhancement, priority:high, size:L |
| [#162](https://github.com/Mozcko/miniGameEngine/issues/162) | ✅ [Python bindings v1 (`import mge`)](PYTHON.md) via nanobind | enhancement, priority:high, size:L |
| [#163](https://github.com/Mozcko/miniGameEngine/issues/163) | ✅ `mge new --lang c\|python`: project templates for C and Python | enhancement, priority:medium, size:M |

</details>
