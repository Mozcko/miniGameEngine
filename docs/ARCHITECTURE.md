# Architecture

This document explains how the engine and the sandbox game fit together at runtime. For a
per‑file reference, see the `README.md` in each folder under `src/`.

## 1. Big picture

```
 game/                              src/mge/  (one static library per folder)
 ───────────────                             ─────────────────────────────────────────────────────────
 sandbox (exe) ──────────────────────────►  mge_main      main() → createApplication() → run()      ─┐
   SandboxApp : mge::Application             mge_platform  PlatformModule: Window, GL ctx, GLEW,       │
     addModule<PlatformModule>() ────────►                SDL event pump → InputState/ActionMap ─────┤
     addModule<AudioModule>()   ─────────►  mge_audio     AudioModule: AudioManager (SDL3 audio) ────┤
 sandbox_world (lib, headless)        ─►  mge_voxel     BlockRegistry, VoxelWorld, ChunkMesher ──┐   ▼
   SandboxBlocks, SandboxTerrain             mge_procgen   MeshData, primitives, noise, SDF ───────┴► mge_core
 SandboxApp ─────────────────────────────►  mge_render    Mesh, Texture, GLSLProgram, Cameras ──►  mge_core
                                                                                                   Application runner + Module,
                                                                                                   Log, Time  (no SDL, no GPU)
 Third party: SDL3 · GLEW/OpenGL 3.3 · GLM · stb_image (core: none)
```

Each engine folder is its own static library (see `src/mge/README.md`), and the game is a
separate executable plus a `sandbox_world` library so its logic can be tested.
`-DMGE_BUILD_MODULES=OFF` builds only the headless modules (`mge_core`, `mge_procgen`,
`mge_voxel`), which CI checks on a machine without SDL.
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
     and resets the input state.
   - `AudioModule` calls `AudioManager::init()` (SDL3 audio, default device; failure → silent, not
     fatal).
2. The game's `onInit()` runs, with modules ready (for example a GL context exists). If it returns
   false, all modules are shut down and `run()` returns 1.

## 4. Frame loop

```
while running:
    for module in modules:  module.onFrameBegin(app)   ← Platform: input.beginFrame, poll SDL events
                                                          (QUIT → requestStop, window-event callback, input)
    if a module requested stop: break                  ← no update/render for that frame
    raw = clock.now() − last  (clamped to maxFrameTime = 0.25 s)
    accumulator += raw × timeScale
    while accumulator ≥ 1/tickRate and steps < 5:      ← fixed timestep (extra backlog is dropped)
        world.run(Fixed); onFixedUpdate(1/tickRate)
    alpha = accumulator / (1/tickRate)
    world.run(Update); onUpdate(raw × timeScale)
    world.run(Render); onRender()
    for module in modules:  module.onFrameEnd(app)     ← Platform: swap buffers
    frameCount++ ; stop when frameCount == config.maxFrames (if > 0)
```

The game reads input through `PlatformModule::actions()` / `input()` (core `ActionMap` / `InputState`).
Raw SDL events (`setEventCallback`) are only used for window events.

Time is app‑owned (`app.time()`, see `src/mge/core/README.md`). The sandbox has no fixed‑step
simulation yet: it moves the camera and streams chunks in `onUpdate(dt)`. Pausing (`P`) sets the
time scale to 0, which freezes the camera (scaled `dt` = 0) while tile animations and the FPS log
use `time().unscaledDelta()`.

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
4. **Opaque pass** – for every loaded chunk inside the camera frustum (`isAabbVisible`, #24): set `model = translate(chunkPos)`, draw opaque mesh.
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

## 7. Voxel world (`mge_voxel` + game)

The engine module [`mge_voxel`](../src/mge/voxel/README.md) owns the data and the meshing; the game supplies content.

- **Blocks:** `BlockRegistry` (id 0 = air). Each `BlockDesc` sets solid, opaque, shape (Cube /
  Cross / None), a colour (procedural path) and/or atlas tiles per face (asset path). The sandbox
  registers 20 blocks with both (`game/SandboxBlocks.cpp`).
- **Chunks:**
  - 16³ `BlockId`s, stacked in all three axes (`ChunkCoord {x, y, z}`).
  - Block `(x, y, z)` occupies `[x, x+1)³`.
  - Coordinates go world → chunk → local with floor division, so negatives work.
- **World:** `VoxelWorld` maps `ChunkCoord` → chunk.
  - It generates chunks through a `TerrainGenerator`, which must be deterministic and independent
    of load order.
  - It answers `getBlock` / `setBlock` / `isSolid` in world coordinates; unloaded chunks read as air.
  - It keeps dirty flags: a new chunk dirties its loaded neighbours, and `setBlock` on a border also
    dirties the neighbour.
- **Generation (sandbox):** `SandboxTerrain`, from a seed.
  - Height = simplex fbm hills + ridged mountains under a low‑frequency mask, in a 64‑block world
    (4 vertical chunks). Water is at 20.
  - Biomes come from height plus temperature/moisture noise: plains (grass, trees, flowers),
    desert, mountains, shore.
  - 3D‑noise tunnel caves, which never break the floor or the surface.
  - Each column is a pure function; a `Pcg32` per column (stream 1 terrain, stream 2 decoration)
    gives a deterministic world with no global RNG state.
  - Trees from columns up to 2 blocks outside the chunk are replayed too, so they cross borders intact.
- **Meshing:** `buildChunkMesh(world, coord)` returns opaque and transparent `MeshData` in
  chunk‑local space.
  - A face is hidden only by an opaque cube, or by the same transparent block (water against water).
  - Cross shapes are used for plants.
  - Neighbours across borders are read from the world. Loading a chunk dirties its loaded
    neighbours, so seams heal on the next re‑mesh.
  - Greedy meshing merges faces of colour‑only cubes. Optional per‑vertex AO is written into the colour.
- **Streaming** (`SandboxApp::streamChunks`, every frame, synchronous):
  - Load every chunk column (4 chunks tall) within 5 of the camera.
  - Mesh the dirty ones within 4 (9×9), then upload with `Mesh::init(MeshData)`.
  - The extra loaded ring means borders mesh against real data.
  - Chunks are never unloaded (KI#4).
- **Collision:** `Camera3D` moves axis by axis (X, Z, Y) and asks a callback whether its AABB
  (±0.3 around the eye) overlaps any solid block. The sandbox converts the box corners with
  `blockAt` and asks `VoxelWorld::isSolid`.

## 8. Static singletons

| Class | Namespace | State | Lifetime |
|-------|-----------|-------|----------|
| `Time` | `mge` | last tick, dt, raw dt, time scale | `init()` called by the game |
| `AudioManager` | `mge` | device id, WAV cache by path, active `SDL_AudioStream`s | `AudioModule` init/shutdown |
| `ResourceManager` | `mge` | `Texture` cache by path | never cleared automatically (unused) |

## 9. Ownership summary

| Object | Owner | How freed |
|--------|-------|-----------|
| `Application` | `main()` (`std::unique_ptr`) | RAII after `run()` returns |
| Modules | `Application::m_modules` | `onShutdown` in reverse order, then destroyed with the app |
| `Window` | `PlatformModule` (unique_ptr) | reset in `PlatformModule::onShutdown` |
| Chunk data | `VoxelWorld` (`unique_ptr` per chunk) | `unloadChunk` or with the world |
| Chunk GPU meshes | `SandboxApp::m_gpuChunks` (`unique_ptr<Mesh>`) | cleared in `SandboxApp::onShutdown` (GL context still alive) |
| GL buffers | `Mesh` | `~Mesh` (Mesh is **not** copy‑safe – copying would double free) |
| GL textures | nobody automatically | `Texture::destroy()` must be called explicitly |
| GL programs | `GLSLProgram` | never deleted (destructor is empty) |
