# CLAUDE.md – context for AI assistants

Read this first. Deeper docs: `README.md` (index), `docs/ARCHITECTURE.md`, `docs/KNOWN_ISSUES.md`,
and a `README.md` inside every folder of `src/` and `assets/`.

## Direction (read before changing anything)
- Goal: **headless C++ core** + optional modules; games made from **pure math/code**, shipped as a
  **single executable with zero asset files**. Full plan: `docs/ROADMAP.md` (GitHub milestones M0–M8,
  issues #1–#67, epics #68–#76 on Mozcko/miniGameEngine).
- **GPU API decision: SDL3 GPU API (Vulkan/Metal/D3D12). Do NOT build new features on OpenGL** – the
  GL renderer is legacy and gets deleted after parity (#40). Platform is on SDL3 (#9 done).
- Workflow & conventions: `CONTRIBUTING.md` (branch `type/<issue>-slug`, conventional commits, one issue
  per PR with "Closes #N", merge only when CI is green). Decisions: `docs/adr/` (0001 SDL3 GPU,
  0002 zero assets, 0003 headless core) – supersede with a new ADR, never edit accepted ones.

## What this is (current state, pre-migration)
- C++17 mini game engine: **SDL3** (window/input/events/audio), OpenGL 3.3 Core via GLEW (legacy
  renderer until #40), GLM math, stb_image (textures). Linux/Fedora dev box, CMake build. SDL3 is
  used from the system (`SDL3-devel`) or built from source via FetchContent (pinned 3.4.18).
- Demo client: `games/sandbox/SandboxApp.cpp` – Minecraft‑like voxel world (16×16×16 chunks,
  procedural sine/cosine terrain, trees/flowers, animated water/lava; no HUD since #9 – FPS goes to the Debug log).
- Owner writes code comments in **Spanish**; keep that style in code. Docs are in English.

## Build & run
```bash
cmake --preset dev && cmake --build --preset dev   # Debug + warnings → build/ (also: release, ci)
cd build && ./sandbox        # cwd MUST be build/: assets use "../assets/..." paths
```
Must build **warning‑free** (`-Wall -Wextra -Wshadow`; CI uses `-Werror` via the `ci` preset). Default build type is Release. CI: GitHub Actions (`.github/workflows/ci.yml`, GCC + Clang, `ci` preset) must be green before merging.
Sanitizers: `asan` preset (ASan+UBSan; locally use Clang – GCC's libasan isn't installed here).
Smoke tests (CTest): `examples/headless_smoke` (no modules, 1000 frames) and the sandbox 120 frames offscreen (label `gpu`, uses `MGE_MAX_FRAMES`). CI jobs: gcc, clang, sanitizers, core-only, lint.
Tests: doctest + CTest in `tests/` (`ctest --preset dev`), headless – no GPU/display needed.
`CMakeLists.txt` uses `file(GLOB_RECURSE ...)` – new .cpp files need a CMake re‑configure.

## Layout & targets
- `src/mge/<module>/` = one static lib each: `mge_core` (headless, **no SDL/GPU** – CI `core-only` job
  builds it with `-DMGE_BUILD_MODULES=OFF`), `mge_platform` (PlatformModule), `mge_render`, `mge_audio` (AudioModule), `mge_main` (main(), core‑only).
- `games/sandbox/` = `sandbox_world` lib (Block/Chunk) + `sandbox` exe (written to the build dir root).
- Tests: `tests/core/` → `mge_core_tests` (core only), `tests/sandbox/` → `sandbox_tests`.
- Explicit source lists per target CMakeLists (no globbing) – add new files there.

## Architecture in 10 lines
1. `src/mge/app/Main.cpp` (`mge_main`) owns `main()` → `mge::createApplication()` (game) → `return app->run()`.
2. `mge::Application` (core, `<mge/core/Application.h>`) = runner + `Module`s. The game adds modules in its
   ctor (`addModule<PlatformModule>()`, `addModule<AudioModule>()`); none = headless. Config: `ApplicationConfig`.
3. `run()`: modules `onInit` in order → game `onInit()` (bool) → loop { modules `onFrameBegin` (Platform polls
   SDL, QUIT → stop, event callback), `onUpdate(dt)`, `onRender()`, modules `onFrameEnd` (swap) } → game
   `onShutdown()` → initialised modules `onShutdown` in reverse. Returns 0 / 1 (init failed).
4. Game stops with `requestStop()`; raw SDL events via `PlatformModule::setEventCallback` (until #15).
5. Static singletons: `InputManager`, `Time`, `AudioManager`, `ResourceManager`.
6. `Mesh` = VAO/VBO/EBO of `Vertex{vec3 pos, vec4 color, vec2 uv}`. **Attrib loc 0 = position, loc 1 = UV; color is NOT sent** (see Mesh.cpp).
7. `Sprite : Mesh` (2D quad from an atlas tile; rebuilds its whole mesh on any setter).
8. `Texture` is a copyable handle; destructor does NOT free GPU memory (call `destroy()`).
9. Cameras: `Camera`(abstract) → `Camera2D`; separate `Camera3D` (free‑fly + AABB collision callback) → `FirstPersonCamera` (gravity/jump; currently unused).
10. Game rendering: opaque chunk meshes, then transparent meshes with depth writes off.

## Conventions
- **One namespace `mge`** (the sandbox's Block/Chunk currently live in it too). Naming, enforced by
  `.clang-tidy` in the CI `lint` job: types `PascalCase`, functions/methods `camelCase`,
  private/protected members `m_camelCase`, static members `s_camelCase`, static constants
  `kPascalCase`, public data fields `camelCase`, macros `UPPER_CASE`.
- Formatting: `.clang-format`, checked in CI with clang-format **22.1.8**. Before committing, run
  **`scripts/lint.sh --fix`**: the same format + naming checks as CI, including untracked new files.
  It needs `cmake --preset ci` first.
- Include roots `src/` and `games/`; always root‑relative: `#include <mge/core/Time.h>`, `#include <sandbox/Chunk.h>`.
- Block face index: 0 Front(+Z), 1 Back(−Z), 2 Left(−X), 3 Right(+X), 4 Top(+Y), 5 Bottom(−Y).
- Blocks are unit cubes **centered** on integer coords (±0.5) → world→block uses `floor(v + 0.5f)`.
- Tileset: 512×512 px, 32 px tiles, 16 cols, tile index row‑major from the **top‑left**.

## Logging
`#include <mge/core/Log.h>` → `MGE_LOG_INFO/WARN/ERROR/FATAL("Category", "text ", value)`, `MGE_ASSERT`
(Debug), `MGE_VERIFY` (always, returns bool). No `std::cout`/`cerr`/`printf`/`exit()` anywhere: log
and return an error instead. `Window::isValid()`, `GLSLProgram::compileShaders/linkShaders` return
success.

## Gotchas you will trip on (details in docs/KNOWN_ISSUES.md)
- `Camera3D::isSphereVisible` always returns true (frustum culling is a stub); chunks are never unloaded.
- SDL3 + GLEW: on Wayland/EGL `glewInit` returns `GLEW_ERROR_NO_GLX_DISPLAY`; `Window` treats it as
  success (functions are already loaded). Headless runs: `SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy`.
- `GLSLProgram::addAttribute` is effectively ignored (shaders use `layout(location=…)`).
- `Window::init` disables depth test; SandboxApp re‑enables it in `onInit`.
- `SDL_Quit()` is called by both `~Window` and `~Application`.
- SandboxApp ignores the `dt` passed to `onUpdate` and uses `Time::` instead.
- Unused code/assets: ResourceManager, Animation, FirstPersonCamera, AudioManager calls,
  `color.*` shaders, sounds/, music/, levels/ (leftovers from an earlier 2D platformer).
