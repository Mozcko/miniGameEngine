# Architecture

This document explains how the engine and the sandbox game fit together at runtime. For a
per‑file reference, see the `README.md` in each folder under `src/`.

## 1. Big picture

```
 games/sandbox/                              src/mge/  (one static library per folder)
 ───────────────                             ─────────────────────────────────────────────────────────
 sandbox (exe) ──────────────────────────►  mge_main      main() → createApplication() → run()      ─┐
   SandboxApp : mge::Application             mge_platform  PlatformModule: Window, GL ctx, GLEW,       │
     addModule<PlatformModule>() ────────►                InputManager, SDL event pump ──────────────┤
     addModule<AudioModule>()   ─────────►  mge_audio     AudioModule: AudioManager (SDL3 audio) ────┤
 sandbox_world (lib)                                                                                  ▼
   Chunk, Block ─────────────────────────►  mge_render    Mesh, Texture, GLSLProgram, Cameras ──►  mge_core
                                                          (→ mge_platform, temporary until #15)    Application runner + Module,
                                                                                                   Log, Time  (no SDL, no GPU)
 Third party: SDL3 · GLEW/OpenGL 3.3 · GLM · stb_image (core: none)
```

Each engine folder is its own static library (see `src/mge/README.md`), and the game is a
separate executable plus a `sandbox_world` library so its logic can be tested.
`-DMGE_BUILD_MODULES=OFF` builds only `mge_core`, which CI checks on a machine without SDL.
The game plugs in through the `Application` / `createApplication` contract.

## 2. The entry‑point contract

```cpp
// src/mge/app/Main.cpp  (target mge_main – core only, no SDL)
int main(int, char**) {
    std::unique_ptr<mge::Application> app(mge::createApplication());   // implemented by the game
    return app->run();                                                  // 0 = ok, 1 = init failed
}
```

A game:
1. Subclasses `mge::Application` (`<mge/core/Application.h>`), passing an `ApplicationConfig`
   (title, size, `headless`, `maxFrames`).
2. Adds the **modules** it needs in its constructor, for example `addModule<PlatformModule>()`
   (window, GL, events) and `addModule<AudioModule>()`. A headless program adds none.
3. Overrides the hooks it needs: `bool onInit()`, `onUpdate(float dt)`, `onRender()`,
   `onShutdown()`. None are mandatory.
4. Defines `mge::Application* mge::createApplication()`.
5. Stops the loop with `requestStop()`.

## 3. Startup sequence – `Application::run()` (`src/mge/core/Application.cpp`)

1. Each module's `onInit(app)` runs **in the order the modules were added**. If one returns false,
   the modules already initialised are shut down in reverse and `run()` returns 1.
   - `PlatformModule` creates the `Window` from the config: SDL video, OpenGL **3.3 Core**
     double‑buffered resizable window, `glewInit()` (EGL‑tolerant), blending on, depth test off,
     and `InputManager::init`.
   - `AudioModule` calls `AudioManager::init()` (SDL3 audio, default device; failure → silent, not
     fatal).
2. The game's `onInit()` runs, with modules ready (for example a GL context exists). If it returns
   false, all modules are shut down and `run()` returns 1.

## 4. Frame loop

```
while running:
    dt = steady_clock delta (seconds)
    for module in modules:  module.onFrameBegin(app)   ← Platform: InputManager::update, poll SDL events
                                                          (QUIT → requestStop, game event callback, input)
    if a module requested stop: break                  ← no update/render for that frame
    onUpdate(dt)
    onRender()
    for module in modules:  module.onFrameEnd(app)     ← Platform: swap buffers
    frameCount++ ; stop when frameCount == config.maxFrames (if > 0)
```

The game receives raw SDL events through `PlatformModule::setEventCallback` until input actions
land in #15.

There is still a second timer, `mge::Time`, which adds a **time scale** (pause/slow‑mo) and
clamps hitches to 0.1 s. SandboxApp calls `Time::update()` at the top of `onUpdate` and ignores
the `dt` argument. #12 replaces both with a single fixed‑timestep clock. Pausing sets the time
scale to 0, which freezes the camera while tile animations keep running on unscaled time.

## 5. Shutdown sequence

After the loop: the game's `onShutdown()` runs first. SandboxApp frees its chunk meshes there,
while the GL context still exists. Then each **initialised** module's `onShutdown(app)` runs in
**reverse order**: Audio closes the device, Platform destroys the window and GL context. Finally
`main()` destroys the `Application`.

SDL lifetime (#17): each module owns its SDL subsystem (`PlatformModule` → video,
`AudioManager` → audio) via `SDL_InitSubSystem`/`SDL_QuitSubSystem`. Whichever module releases the
last subsystem calls `SDL_Quit()`, exactly once. The game's GL resources are freed in `onShutdown`,
before the window and context go away. The full lifecycle runs clean under ASan/UBSan
(`asan` preset).

## 6. Rendering pipeline

### Vertex format and Mesh
`mge::Vertex { vec3 position; vec4 color; vec2 uv; }` (36 bytes, interleaved).
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
| `m_shader3D` | `basic3d.vert/.frag` | `model`, `view`, `projection`, `myTexture`, `baseColor` | Chunks |
| – | `color.vert/.frag` | `P` | nothing (legacy) |

### SandboxApp frame (onRender)
1. Clear to sky blue `(0.5, 0.7, 1.0)`, clear color + depth.
2. Bind `m_shader3D`; upload camera projection & view, `baseColor = white`, sampler unit 0.
3. Bind the tileset texture.
4. **Opaque pass** – for every loaded chunk: set `model = translate(chunkPos)`, draw opaque mesh.
5. **Transparent pass** – `glDepthMask(GL_FALSE)`, draw each chunk's transparent mesh
   (water, lava, glass, leaves, flowers…), restore depth mask. No back‑to‑front sorting.
6. Every 5 s the average FPS is written to the Debug log (the text HUD was removed in #9).

Global GL state set in `SandboxApp::onInit`: depth test on, back‑face culling on (CCW = front),
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
- **Streaming** (`SandboxApp::onUpdate`): every frame, any missing chunk within a 9×9 area
  (`renderDist = 4`) around the camera is created synchronously. Chunks are stored in
  `std::map<ChunkKey, Chunk*>` and never unloaded.
- **Collision**: `Camera3D` moves axis by axis (X, Z, Y) and asks a callback whether its AABB
  (±0.3 around the eye) overlaps any solid block; SandboxApp implements the callback with
  `getGlobalBlock()`.

## 8. Static singletons

| Class | Namespace | State | Lifetime |
|-------|-----------|-------|----------|
| `InputManager` | `mge` | mouse pos/delta, cursor lock, window ptr | `PlatformModule` init/shutdown |
| `Time` | `mge` | last tick, dt, raw dt, time scale | `init()` called by the game |
| `AudioManager` | `mge` | device id, WAV cache by path, active `SDL_AudioStream`s | `AudioModule` init/shutdown |
| `ResourceManager` | `mge` | `Texture` cache by path | never cleared automatically (unused) |

## 9. Ownership summary

| Object | Owner | How freed |
|--------|-------|-----------|
| `Application` | `main()` (`std::unique_ptr`) | RAII after `run()` returns |
| Modules | `Application::m_modules` | `onShutdown` in reverse order, then destroyed with the app |
| `Window` | `PlatformModule` (unique_ptr) | reset in `PlatformModule::onShutdown` |
| `Chunk*` | `SandboxApp::m_chunkMap` | deleted in `SandboxApp::onShutdown` (GL context still alive) |
| `Mesh*` inside Chunk | Chunk | deleted in `~Chunk` |
| GL buffers | `Mesh` | `~Mesh` (Mesh is **not** copy‑safe – copying would double free) |
| GL textures | nobody automatically | `Texture::destroy()` must be called explicitly |
| GL programs | `GLSLProgram` | never deleted (destructor is empty) |
