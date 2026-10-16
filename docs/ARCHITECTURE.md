# Architecture

This document explains how the engine and the sandbox game fit together at runtime. For a
per‑file reference, see the `README.md` in each folder under `src/`.

## 1. Big picture

```
 game/                                     src/mge/  (one static library per folder)
 ────────────────────────                  ──────────────────────────────────────────────────────────────
 sandbox (exe) ─────────────────────────►  mge_main      main() → createApplication() → run()
   SandboxApp : mge::Application           mge_platform  PlatformModule: Window, SDL events → InputState/ActionMap
     addModule<PlatformModule>()           mge_gpu       GpuModule: SDL3 GPU device + swapchain, RAII handles
     addModule<GpuModule>()                mge_graphics  MeshRenderer, Texture2D (embedded shaders)
     addModule<AudioModule>()              mge_audio     AudioModule: AudioManager (SDL3 audio)
 sandbox_world (lib, headless) ─────────►  mge_voxel     BlockRegistry, VoxelWorld, ChunkMesher
   SandboxBlocks, SandboxTerrain           mge_procgen   MeshData, primitives, noise, SDF
                                           mge_core      Application runner + Module, ECS, cameras, Input, Log, Time
                                                         (no SDL, no GPU)
 Third party: SDL3 (incl. SDL GPU → Vulkan) · glslang (build time) · GLM · stb_image (core: GLM only)
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
   (window, events), `addModule<GpuModule>()` (SDL3 GPU) and `addModule<AudioModule>()`. A
   headless program adds none.
3. Overrides the hooks it needs: `bool onInit()`, `onUpdate(float dt)`, `onRender()`,
   `onShutdown()`. None are mandatory.
4. Defines `mge::Application* mge::createApplication()`.
5. Stops the loop with `requestStop()`.

## 3. Startup sequence – `Application::run()` (`src/mge/core/Application.cpp`)

1. Each module's `onInit(app)` runs **in the order the modules were added**. If one returns false,
   the modules already initialised are shut down in reverse and `run()` returns 1.
   - `PlatformModule` initialises SDL video and creates the resizable `Window` from the config.
   - `GpuModule` creates the SDL GPU device (Vulkan; validation in Debug), claims the window and
     sets the present mode from `config.vsync`.
   - `AudioModule` calls `AudioManager::init()` (SDL3 audio, default device; failure → silent, not
     fatal).
2. The game's `onInit()` runs, with modules ready (for example the GPU device exists). If it returns
   false, all modules are shut down and `run()` returns 1.

## 4. Frame loop

```
while running:
    for module in modules:  module.onFrameBegin(app)   ← Platform: input.beginFrame, poll SDL events
                                                          (QUIT → requestStop, window-event callback, input)
                                                          Gpu: acquire command buffer + swapchain image,
                                                          app.setViewport(swapchain size)
    if a module requested stop: break                  ← no update/render for that frame
    raw = clock.now() − last  (clamped to maxFrameTime = 0.25 s)
    accumulator += raw × timeScale
    while accumulator ≥ 1/tickRate and steps < 5:      ← fixed timestep (extra backlog is dropped)
        world.run(Fixed); onFixedUpdate(1/tickRate)
    alpha = accumulator / (1/tickRate)
    world.run(Update); onUpdate(raw × timeScale)
    CameraSystem::update(registry, viewport)            ← autoAspect cameras follow the window
    world.run(Render); onRender()
    for module in modules:  module.onFrameEnd(app)     ← Gpu: submit (SDL presents)
    frameCount++ ; stop when frameCount == config.maxFrames (if > 0)
```

The game reads input through `PlatformModule::actions()` / `input()` (core `ActionMap` / `InputState`).
Raw SDL events (`setEventCallback`) are only used for window events.

Time is app‑owned (`app.time()`, see `src/mge/core/README.md`). The sandbox has no fixed‑step
simulation yet: it moves the camera and streams chunks in `onUpdate(dt)`. Pausing (`P`) sets the
time scale to 0, which freezes the camera (scaled `dt` = 0) while tile animations and the FPS log
use `time().unscaledDelta()`.

## 5. Shutdown sequence

After the loop: the game's `onShutdown()` runs first. SandboxApp frees its GPU meshes, atlas and
renderer there, while the GPU device still exists. Then each **initialised** module's
`onShutdown(app)` runs in **reverse order**: Audio closes the device, Gpu waits for the GPU,
releases the window and destroys the device, then Platform destroys the window. Finally
`main()` destroys the `Application`.

SDL lifetime (#17): each module owns its SDL subsystem (`PlatformModule` → video,
`AudioManager` → audio) via `SDL_InitSubSystem`/`SDL_QuitSubSystem`. Whichever module releases the
last subsystem calls `SDL_Quit()`, exactly once. The game's GPU resources are freed in
`onShutdown`, before the device goes away. The full lifecycle runs clean under ASan/UBSan
(`asan` preset).

## 6. Rendering pipeline (SDL3 GPU, #28–#30)

The engine renders through the **SDL3 GPU API** (Vulkan; Metal/D3D12 later). The OpenGL/GLEW
renderer was deleted in #40. Cameras are core ECS components (#32).

### Frame flow
1. `PlatformModule::onFrameBegin`: poll SDL events, update the input.
2. `GpuModule::onFrameBegin`: acquire a command buffer and the swapchain image (`frame()`; null
   while minimised).
3. Game `onFixedUpdate` / `onUpdate`. Uploads, such as chunk meshes or atlas tiles, are copy passes
   on their own command buffers.
4. Game `onRender`: `MeshRenderer::begin` opens a render pass (clear colour + depth), the game
   issues `draw` calls, `end` closes it.
5. `GpuModule::onFrameEnd`: submit; SDL presents.

### Vertex format
`GpuVertex { vec3 position; vec3 normal; vec4 color; vec2 uv; }` (48 bytes, interleaved, from
`MeshData` via `device.uploadMesh`). Shader locations: 0 position, 1 normal, 2 colour, 3 uv. All
four are uploaded; the GL path never sent the colour (KI#13).

### Shaders
GLSL compiled to SPIR‑V at build time and embedded (`mge_add_shaders`, #29). No shader files are
read at runtime.

| Shader | Where | Inputs |
|--------|-------|--------|
| `mesh.vert/.frag` | `mge_graphics` | vertex UBO `{viewProjection, model}` (set 1); fragment sampler `albedo` (set 2); output `texture × vertexColor` |
| `triangle.*` | `examples/gpu_triangle` | vertex UBO `{angle, aspect}` |

### Sandbox frame (`onRender`)
1. `MeshRenderer::begin` clears to sky blue `(0.5, 0.7, 1.0)` and clears depth (D32 or D24).
2. Camera: `cameraMatrices(transform, camera, viewport())` from the camera entity. Its aspect is
   set by `CameraSystem`; the projection uses SDL GPU's NDC (Y up, Z in [0, 1]).
3. **Opaque pass**: pipeline with depth test and write, back‑face culling (CCW = front), no
   blending. Every chunk inside the camera frustum (`CameraMatrices::frustum`) is drawn with
   `model = translate(chunkOrigin)` and the atlas.
4. **Transparent pass**: alpha blending, depth test without writes. Draws water, lava, glass,
   leaves and plants, with no sorting; the cutout pass and sorting come in #31.
5. Every 5 s the average FPS is written to the Debug log.

### Textures
`Texture2D::fromFile` (stb_image → RGBA8) stores the image flipped vertically, so the engine's
GL‑style UVs (v up) keep working, and keeps a CPU copy. The water and lava animation copies
32×32 tiles from that original image every 0.2 s (`copyFromOriginal` → a copy‑pass upload) into
tiles 207 / 239. Sampling is nearest, clamped to the edge.

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
  - Mesh the dirty ones within 4 (9×9), then upload with `device.uploadMesh`.
  - The extra loaded ring means borders mesh against real data.
  - Chunks are never unloaded (KI#4).
- **Collision:** `FlyController` (core) moves axis by axis (X, Z, Y) and asks a callback whether its AABB
  (±0.3 around the eye) overlaps any solid block. The sandbox converts the box corners with
  `blockAt` and asks `VoxelWorld::isSolid`.

## 8. Static singletons

| Class | Namespace | State | Lifetime |
|-------|-----------|-------|----------|
| `Time` | `mge` | last tick, dt, raw dt, time scale | `init()` called by the game |
| `AudioManager` | `mge` | device id, WAV cache by path, active `SDL_AudioStream`s | `AudioModule` init/shutdown |

## 9. Ownership summary

| Object | Owner | How freed |
|--------|-------|-----------|
| `Application` | `main()` (`std::unique_ptr`) | RAII after `run()` returns |
| Modules | `Application::m_modules` | `onShutdown` in reverse order, then destroyed with the app |
| `Window` | `PlatformModule` (unique_ptr) | reset in `PlatformModule::onShutdown`, after `GpuModule` released it |
| Chunk data | `VoxelWorld` (`unique_ptr` per chunk) | `unloadChunk` or with the world |
| Chunk GPU meshes | `SandboxApp::m_gpuChunks` (`GpuMesh`: move‑only GPU buffers) | cleared in `SandboxApp::onShutdown`, before the `GpuModule` destroys the device |
| GPU device | `GpuModule` (`unique_ptr<GpuDevice>`) | `onShutdown`: wait idle, release the window, destroy |
| GPU buffers, textures, samplers, pipelines | Move‑only `GpuHandle`s (`GpuMesh`, `Texture2D`, `MeshRenderer` members) | released by their destructors, before the `GpuDevice` |
