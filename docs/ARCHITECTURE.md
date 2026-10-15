# Architecture

This document explains how the engine and the sandbox game fit together at runtime. For a
per‑file reference, see the `README.md` in each folder under `src/`.

## 1. Big picture

```
 games/sandbox/                         src/mge/  (one static library per folder)
 ───────────────                        ────────────────────────────────────────────
 sandbox (exe) ─────────────────────►  mge_main  (OBJECT: main() → createApplication)
   SandboxApp : Engine::Application ─►  mge_app   Application loop ─┬─► mge_platform  Window, GL ctx, GLEW, InputManager ─┐
 sandbox_world (lib)                                                └─► mge_audio     AudioManager (SDL_mixer)           │
   Chunk, Block ────────────────────►  mge_render  Mesh, Texture, GLSLProgram, Cameras … ──(temp)──► mge_platform        │
                                                     │                                                                   ▼
                                                     └──────────────────────────────────────────────────────────►  mge_core  Time
                                                                                                                (no SDL, no GPU)
 Third party: SDL2 · SDL2_mixer · SDL2_ttf · GLEW/OpenGL 3.3 · GLM · stb_image (core: none)
```

Each engine folder is its own static library (see `src/mge/README.md`), and the game is a
separate executable plus a `sandbox_world` library so its logic can be tested.
`-DMGE_BUILD_MODULES=OFF` builds only `mge_core`, which CI checks on a machine without SDL.
The game plugs in through the `Application` / `CreateApplication` contract.

## 2. The entry‑point contract

```cpp
// src/mge/app/Main.cpp
int main(int, char**) {
    auto app = Engine::CreateApplication();   // implemented by the game
    app->Run();
    delete app;
}
```

A game must:
1. Subclass `Engine::Application` and implement `OnInit()`, `OnUpdate(float dt)`,
   `OnRender()` (pure virtual) and optionally `OnEvent(SDL_Event&)`.
2. Define `Engine::Application* Engine::CreateApplication()` returning a heap instance.
3. Stop the loop by setting the protected member `m_Running = false`.

## 3. Startup sequence

`Engine::Application::Application()` (`src/mge/app/Application.cpp`):

1. `MiniGameEngine::AudioManager::init()` – `Mix_OpenAudio(44100, default, stereo, 2048)`.
2. `SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER)`.
3. `m_Window = make_unique<Window>("Mini Game Engine - Minecraft Cube", 1280, 720)`:
   - requests an OpenGL **3.3 Core**, double‑buffered context, resizable window;
   - runs `glewInit()` (with `glewExperimental = GL_TRUE`);
   - enables alpha blending, **disables** depth testing (2D default), prints GL version.
4. `TTF_Init()`.

Then the derived constructor runs (SandboxApp constructs its `Camera3D` and `Camera2D` with
1280×720).

## 4. Frame loop

`Application::Run()`:

```
InputManager::Init(sdlWindow)
OnInit()                                   ← game loads shaders, textures, fonts, GL state
lastFrameTime = SDL_GetPerformanceCounter()
while (m_Running):
    CalculateDeltaTime()                   ← m_DeltaTime (seconds, unclamped)
    InputManager::Update()                 ← reset mouse delta, re‑assert cursor lock
    for each SDL_Event:
        SDL_QUIT → m_Running = false
        OnEvent(event)                     ← game first
        InputManager::ProcessEvent(event)  ← then input accumulates mouse motion
    OnUpdate(m_DeltaTime)
    OnRender()
    window.swapBuffers()
```

Note: there is a second, independent timer – `MiniGameEngine::Time` – which supports a
**time scale** (pause/slow‑mo) and clamps big hitches to 0.1 s. SandboxApp calls
`Time::update()` at the top of `OnUpdate` and uses `Time::getDeltaTime()` instead of the `dt`
argument. Pausing sets the time scale to 0, which freezes the camera (camera `update` returns early
for dt ≈ 0) while tile animations keep running on unscaled time.

## 5. Shutdown sequence

`delete app` → `~SandboxApp` (deletes chunks) → member destructors (GUILabels close fonts,
Meshes delete GL buffers, …) → `~Application`: `AudioManager::shutdown()`, `TTF_Quit()`,
`SDL_Quit()` → `m_Window` unique_ptr destroyed: deletes GL context, destroys window, `SDL_Quit()`
again.

Caveat: `~Application` body runs before its own members are destroyed, so `SDL_Quit()` and
`TTF_Quit()` happen before the GL context is deleted by `~Window`. (Derived‑class GL resources
are fine: SandboxApp members are destroyed before `~Application` runs.) It works in practice but
is fragile – see `KNOWN_ISSUES.md`.

## 6. Rendering pipeline

### Vertex format and Mesh
`MiniGameEngine::Vertex { vec3 position; vec4 color; vec2 uv; }` (36 bytes, interleaved).
`Mesh::init(vertices, indices)` uploads to a VAO + VBO + EBO (`GL_STATIC_DRAW`) and sets:

| location | data            | type  |
|----------|-----------------|-------|
| 0        | `position`      | vec3  |
| 1        | `uv`            | vec2  |
| (none)   | `color`         | – (attribute 1 is first configured for color, then overwritten by UV) |

All shaders therefore read `layout(location=0)` position and `layout(location=1)` UV.
2D shaders declare position as `vec2`; they just read the first two floats.

### Shaders in use

| Program | Files | Uniforms | Used by |
|---------|-------|----------|---------|
| `shader3D` | `basic3d.vert/.frag` | `model`, `view`, `projection`, `myTexture`, `baseColor` | Chunks |
| `_guiShader` | `texture.vert/.frag` | `P`, `myTexture` (defaults to unit 0) | GUILabel |
| – | `color.vert/.frag` | `P` | nothing (legacy) |

### SandboxApp frame (OnRender)
1. Clear to sky blue `(0.5, 0.7, 1.0)`, clear color + depth.
2. Bind `shader3D`; upload camera projection & view, `baseColor = white`, sampler unit 0.
3. Bind the tileset texture.
4. **Opaque pass** – for every loaded chunk: set `model = translate(chunkPos)`, draw opaque mesh.
5. **Transparent pass** – `glDepthMask(GL_FALSE)`, draw each chunk's transparent mesh
   (water, lava, glass, leaves, flowers…), restore depth mask. No back‑to‑front sorting.
6. **HUD** – disable depth test, draw FPS and info `GUILabel`s with the 2D shader and the
   `Camera2D` projection, re‑enable depth test.

Global GL state set in `SandboxApp::OnInit`: depth test on, back‑face culling on (CCW = front),
alpha blending `SRC_ALPHA, ONE_MINUS_SRC_ALPHA`.

### Textures
`Texture::load` uses stb_image with vertical flip on load, uploads `GL_RGB`/`GL_RGBA` with
`GL_NEAREST` filtering (pixel‑art look), clamps to edge, generates mipmaps, and **keeps a CPU copy**
of the pixels so `updateTile()` can copy one atlas tile over another at runtime – that is how the
water and lava animation works (every 0.2 s, frames are blitted into tile 207 / 239 via
`glTexSubImage2D`).

## 7. Voxel world (game layer)

- **Chunk** = 16 (X) × 16 (Y) × 16 (Z) `Block`s, world position `(cx*16, 0, cz*16)`.
- **Generation** (`Chunk::generate`): height = `(sin(wx*0.2) + cos(wz*0.2)) * 3 + 6`, clamped
  to [1, 15]. Below the surface: 2 layers dirt, rest stone. Water fills air up to y = 5. Surface is
  sand near water, else grass with 5 % gravel and 0.5 % lava pools. Decoration pass: 2 % trees,
  2 % roses, 2 % flowers on grass. Per column, pass 1 seeds `srand(wx*8921 + wz*33771)` and pass 2
  seeds the same value XOR `0x5bd1e995` (#5) → deterministic world.
- **Meshing** (`Chunk::buildMesh`): face culling against neighbours (cross‑chunk neighbours are
  *predicted* procedurally, not read from the real neighbour chunk), cross‑shaped billboards for
  plants/cobweb, separate opaque / transparent meshes.
- **Streaming** (`SandboxApp::OnUpdate`): every frame, any missing chunk within a 9×9 area
  (`renderDist = 4`) around the camera is created synchronously. Chunks are stored in
  `std::map<ChunkKey, Chunk*>` and never unloaded.
- **Collision**: `Camera3D` moves axis by axis (X, Z, Y) and asks a callback whether its AABB
  (±0.3 around the eye) overlaps any solid block; SandboxApp implements the callback with
  `getGlobalBlock()`.

## 8. Static singletons

| Class | Namespace | State | Lifetime |
|-------|-----------|-------|----------|
| `InputManager` | `Engine` | mouse pos/delta, cursor lock, window ptr | `Init` in `Run()` |
| `Time` | `MiniGameEngine` | last tick, dt, raw dt, time scale | `init()` called by the game |
| `AudioManager` | `MiniGameEngine` | `Mix_Chunk*` / `Mix_Music*` caches by path | Application ctor/dtor |
| `ResourceManager` | `MiniGameEngine` | `Texture` cache by path | never cleared automatically (unused) |

## 9. Ownership summary

| Object | Owner | How freed |
|--------|-------|-----------|
| `Application` | `main()` | `delete app` |
| `Window` | `Application::m_Window` (unique_ptr) | RAII |
| `Chunk*` | `SandboxApp::chunkMap` | deleted in `~SandboxApp` |
| `Mesh*` inside Chunk | Chunk | deleted in `~Chunk` |
| GL buffers | `Mesh` | `~Mesh` (Mesh is **not** copy‑safe – copying would double free) |
| GL textures | nobody automatically | `Texture::destroy()` must be called explicitly |
| `TTF_Font*` | `GUILabel` | `~GUILabel` |
| GL programs | `GLSLProgram` | never deleted (destructor is empty) |
