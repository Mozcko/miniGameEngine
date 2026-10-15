# CLAUDE.md – context for AI assistants

Read this first. Deeper docs: `README.md` (index), `docs/ARCHITECTURE.md`, `docs/KNOWN_ISSUES.md`,
and a `README.md` inside every folder of `src/` and `assets/`.

## Direction (read before changing anything)
- Goal: **headless C++ core** + optional modules; games made from **pure math/code**, shipped as a
  **single executable with zero asset files**. Full plan: `docs/ROADMAP.md` (GitHub milestones M0–M8,
  issues #1–#67, epics #68–#76 on Mozcko/miniGameEngine).
- **GPU API decision: SDL3 GPU API (Vulkan/Metal/D3D12). Do NOT build new features on OpenGL** – the
  GL renderer is legacy and gets deleted after parity (#40). Platform moves SDL2 → SDL3 (#9).
- Branches: `type/<issue>-slug` (e.g. `chore/1-repo-hygiene`); PRs reference "Closes #N".

## What this is (current state, pre-migration)
- C++17 mini game engine: SDL2 (window/input/events), OpenGL 3.3 Core via GLEW, GLM math,
  stb_image (textures), SDL2_ttf (text), SDL2_mixer (audio). Linux/Fedora dev box, CMake build.
- Demo client: `src/Game/SandboxApp.cpp` – Minecraft‑like voxel world (16×16×16 chunks,
  procedural sine/cosine terrain, trees/flowers, animated water/lava, text HUD).
- Owner writes code comments in **Spanish**; keep that style in code. Docs are in English.

## Build & run
```bash
cmake --preset dev && cmake --build --preset dev   # Debug + warnings → build/ (also: release, ci)
cd build && ./miniGameEngine        # cwd MUST be build/: assets use "../assets/..." paths
```
Must build **warning‑free** (`-Wall -Wextra -Wshadow`; CI uses `-Werror` via the `ci` preset). Default build type is Release. CI: GitHub Actions (`.github/workflows/ci.yml`, GCC + Clang, `ci` preset) must be green before merging.
Tests: doctest + CTest in `tests/` (`ctest --preset dev`), headless – no GPU/display needed.
`CMakeLists.txt` uses `file(GLOB_RECURSE ...)` – new .cpp files need a CMake re‑configure.

## Architecture in 10 lines
1. `src/Engine/Main.cpp` owns `main()` → `Engine::CreateApplication()` (defined by the game) → `Run()`.
2. `Engine::Application` ctor: AudioManager::init → SDL_Init → `Window` (1280×720, GL 3.3 core, GLEW) → TTF_Init.
3. `Run()`: InputManager::Init → `OnInit()` → loop { dt, InputManager::Update, poll events
   (→ `OnEvent` + InputManager::ProcessEvent), `OnUpdate(dt)`, `OnRender()`, swapBuffers }.
4. Game stops by setting protected `m_Running = false`.
5. Static singletons: `InputManager`, `Time`, `AudioManager`, `ResourceManager`.
6. `Mesh` = VAO/VBO/EBO of `Vertex{vec3 pos, vec4 color, vec2 uv}`. **Attrib loc 0 = position, loc 1 = UV; color is NOT sent** (see Mesh.cpp).
7. `Sprite : Mesh` (2D quad from an atlas tile; rebuilds its whole mesh on any setter).
8. `Texture` is a copyable handle; destructor does NOT free GPU memory (call `destroy()`).
9. Cameras: `Camera`(abstract) → `Camera2D`; separate `Camera3D` (free‑fly + AABB collision callback) → `FirstPersonCamera` (gravity/jump; currently unused).
10. Game rendering: opaque chunk meshes, then transparent meshes with depth writes off, then HUD.

## Conventions
- Two namespaces coexist: `Engine::` (Application, InputManager, all cameras) and
  `MiniGameEngine::` (everything else, incl. game Block/Chunk). Don't "fix" this casually – it's pervasive.
- Members: `_camelCase` in MiniGameEngine classes, `m_PascalCase`/`s_PascalCase` in Engine classes;
  Camera3D mixes in PascalCase public‑style fields (`Position`, `Yaw`...). Match the file you're in.
- Methods: `camelCase` in MiniGameEngine, `PascalCase` in Engine (InputManager, Application).
- Include root is `src/` (`#include <Engine/Core/Application.h>`), relative includes also used.
- Block face index: 0 Front(+Z), 1 Back(−Z), 2 Left(−X), 3 Right(+X), 4 Top(+Y), 5 Bottom(−Y).
- Blocks are unit cubes **centered** on integer coords (±0.5) → world→block uses `floor(v + 0.5f)`.
- Tileset: 512×512 px, 32 px tiles, 16 cols, tile index row‑major from the **top‑left**.

## Gotchas you will trip on (details in docs/KNOWN_ISSUES.md)
- Trees and roses NEVER spawn: `Chunk::generate` pass 2 re‑seeds `srand` with the same seed, so its
  decoration roll repeats pass 1's gravel roll (always ≥ 5 on grass). Verified headlessly.
- `Camera3D::isSphereVisible` always returns true (frustum culling is a stub); chunks are never unloaded.
- `Camera2D` ortho is in aspect units (−1.78..1.78 × −1..1), but HUD labels are placed in pixels
  (e.g. y=680) and `GL_CULL_FACE` is on while sprite quads wind clockwise → HUD text is probably invisible.
- `GLSLProgram::addAttribute` is effectively ignored (shaders use `layout(location=…)`).
- `Window::init` disables depth test; SandboxApp re‑enables it in `OnInit`.
- `SDL_Quit()` is called by both `~Window` and `~Application`.
- SandboxApp ignores the `dt` passed to `OnUpdate` and uses `Time::` instead.
- Unused code/assets: ResourceManager, Animation, FirstPersonCamera, AudioManager calls,
  `color.*` shaders, sounds/, music/, levels/ (leftovers from an earlier 2D platformer).
