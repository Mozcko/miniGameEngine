# Known issues and technical debt

Found during a full read of the code (state at commit `09a3bfd`, "version1.0 with 2d and 3d
assets"). Severity is a rough guide. Line numbers may drift: search for the quoted code.

## Bugs / likely visible problems

| # | Severity | Where | Issue |
|---|----------|-------|-------|
| 0 | ✅ Fixed (#5) | `game/Chunk.cpp` `generate`, pass 2 | **Trees and roses never spawn.** Both passes call `srand(seed)` with the same seed. In pass 1, a grass column (height > 6) consumes its first `rand()%100` for the gravel test and stays Grass only if it is ≥ 5. Pass 2 re‑seeds and its first `rand()%100` is that **same value**, so `r < 2` (tree) and `r < 4` (rose) can never be true. Only `r == 5` → Flower (~1 %) remains. A headless run over 1,600 chunks produced 0 Log, 0 Leaves, 0 Rose and 1,422 Flower blocks. Fix: use a different seed for pass 2 (e.g. `seed ^ 0x9E3779B9`) or a per‑column hash instead of global `srand/rand`. |
| 18 | 🗑️ Removed (#9); ✅ verified for the new UI (#44): `Renderer2D` text is upright | `render/core/Texture.cpp` `loadFromSurface` + `Sprite.cpp` UVs | **Text renders upside down** once the HUD is visible. SDL surfaces are top‑row‑first and aren't flipped on upload, but `Sprite` maps v = 1 to the quad's top. |
| 19 | ✅ Fixed (#6); ✅ verified for the new UI (#44): the colour is per call | `game/SandboxApp.cpp` `onRender` | **FPS label turns white:** `setText("FPS: …")` uses the default white color, so the yellow from `onInit` is lost on the first refresh. Pass the color every time. |
| 20 | 🗑️ Removed (#40): the GPU path loads RGBA (`stbi_load(…, 4)`) | `render/core/Texture.cpp` `load` | **Buffer over‑read** for 1‑ or 2‑channel images (grayscale or grayscale+alpha PNGs): format is `GL_RGB` for anything that isn't 4 channels, so GL reads 3 bytes per pixel from a smaller buffer. Fix: `stbi_load(..., 4)` and always use `GL_RGBA`. |
| 21 | ✅ Fixed (#31): Cutout pass (alpha test + depth write) for leaves/plants, transparent items sorted back‑to‑front | `game/SandboxApp.cpp` `onRender`, `basic3d.frag` | Cut‑out blocks (Leaves, flowers) are drawn in the transparent pass with depth writes off and no sorting, so a farther leaf can be drawn over a nearer one. This is hidden today because no trees spawn (#0). Fix: `if (color.a < 0.5) discard;` in the shader and draw cut‑outs in the opaque pass; keep only water/lava/glass in the blended pass, sorted back to front. |
| 22 | ✅ Fixed (#37) — GL: `SDL_GL_DEPTH_SIZE` 24; SDL GPU: `GpuDevice::depthFormat()` = D32/D24 | `platform/Window.cpp` `init` | No depth buffer size is requested (`SDL_GL_DEPTH_SIZE`). SDL's default is 16 bits, and with near = 0.1 / far = 1000 precision gets coarse at distance, which can cause z‑fighting. Request 24 bits and/or raise the near plane. Many drivers already return 24, so it may not show on this machine. |
| 23 | ✅ Fixed (#37) — `ApplicationConfig::vsync` (+ `MGE_VSYNC`): GL swap interval; SDL GPU present mode, toggle with `GpuModule::setVsync` | `platform/Window.cpp` | No `SDL_GL_SetSwapInterval`, so the frame rate is uncapped unless the driver forces vsync (100 % GPU/CPU use). |
| 24 | ✅ Fixed (#2) | `CMakeLists.txt` | No default `CMAKE_BUILD_TYPE`, so a plain `cmake -S . -B build` compiles **without optimisation** (no `-O2`) and makes chunk generation and meshing slow. Add a Release default or pass `-DCMAKE_BUILD_TYPE=Release`. No warning flags are enabled either. |
| 25 | ✅ Fixed (#13) | `render/core/GLSLProgram.cpp` `linkShaders` | On link failure the program is deleted but `m_programId` keeps the dead id, so later `use()` calls generate GL errors instead of failing clearly. |
| 26 | ✅ Fixed (#13, #11, #17) | `app/Application.cpp` ctor | If `SDL_Init` fails it returns before creating `m_window`; `run()` then dereferences the null window. Partly fixed in #13: the window is created only if `SDL_Init` succeeds, and `run()` refuses to start (logged) when init failed; full module shutdown order is #17. |
| 27 | ✅ Fixed (#14) | `game/Chunk.cpp` `generate` | `srand(worldX * 8921 + worldZ * 33771)` overflows `int` (undefined behaviour) for coordinates beyond ~±60 000 blocks, and the global `srand` reseeds `rand()` for any other code that uses it. |
| 1 | 🗑️ Removed (#9); ✅ the new UI is pixel‑space, origin top‑left (#42, verified in #44) | `render/camera/Camera2D.cpp` `updateProjection` + `game/SandboxApp.cpp` HUD | `Camera2D` builds `ortho(-aspect*zoom, aspect*zoom, -zoom, zoom)` (≈ −1.78..1.78 × −1..1) but `GUILabel`s are positioned and sized in **pixels** (`setPosition(10, 680)`, sprite size = text surface size). Labels land far outside the view. A pixel HUD needs `ortho(0, width, 0, height)`. |
| 2 | 🗑️ Removed (#40); ✅ the 2D pipeline has no culling, so winding doesn't matter (#42, verified in #44) | `render/2d/Sprite.cpp` `initFromAtlas` | Quad indices `0,1,2 / 2,3,0` over BL, TL, TR, BR are **clockwise**. With `GL_CULL_FACE` enabled by SandboxApp, sprites/labels are culled. Either reverse winding or disable culling for the HUD pass. |
| 3 | ✅ Fixed (#24) | `render/camera/Camera3D.cpp` `isSphereVisible` | Always returns `true` – frustum culling is a stub. Chunk loading and drawing are therefore distance‑only. |
| 4 | ✅ Fixed (#56, #57): `ChunkStreamer` generates/meshes on worker threads, unloads with hysteresis, pools chunk storage | `game/SandboxApp.cpp` `onUpdate` | Chunks are created synchronously and **never unloaded**; memory grows as you explore. A new chunk causes a frame hitch. |
| 5 | ✅ Fixed (#25, #26) | `game/Chunk.cpp` `getNeighborBlock` | Fix: `mge_voxel`'s mesher reads real neighbour blocks from `VoxelWorld`, and loading a chunk marks its loaded neighbours dirty so they re‑mesh. *Original:* Faces on chunk borders are culled using a procedural *guess* (water ≤ 5 else "stone") instead of the neighbour chunk's real blocks: trees, lava, flowers and sand across borders are ignored, so faces may be missing or extra at seams. Water level `5` is duplicated here instead of using `waterLevel`. |
| 6 | ✅ Fixed (#26) | `game/Chunk.cpp` `placeTree` | Leaves outside the chunk are simply dropped → trees cut at chunk borders. Fix: `SandboxTerrain` also replays the trees of columns up to 2 blocks outside the chunk (stateless, in world order), so each chunk writes its share of every tree and the result doesn't depend on load order. |
| 7 | 🗑️ Removed (#9); ✅ the new UI uploads one batched vertex buffer per frame, not per label (#42) | `render/2d/GUILabel.cpp` `draw` | `m_sprite.setPosition()` is called every draw, which **re‑creates the VAO/VBO/EBO** every frame. Also `m_scale` and the computed `model` matrix are unused. |
| 8 | ✅ Fixed (#6) | `game/SandboxApp.cpp` `onRender` | FPS = `1 / Time::getDeltaTime()`; when paused dt = 0 → division by zero, float→int overflow (UB). Use unscaled dt. |
| 9 | 🗑️ Removed (#40) | `render/core/GLSLProgram.cpp` `use/unuse` | Calls `glEnable/DisableVertexAttribArray` with no VAO bound (core profile → `GL_INVALID_OPERATION`), and it would modify whatever VAO is bound. Attribute state already lives in each Mesh's VAO, so this is unnecessary. |
| 10 | 🗑️ Removed (#40) | `GLSLProgram::addAttribute` | `glBindAttribLocation` is overridden by `layout(location=…)` in the shaders; harmless but misleading. |
| 11 | 🗑️ Removed (#40) | `GLSLProgram::getUniformLocation` | (`GL_INVALID_INDEX` comparison fixed in #2.) Prints for every lookup of an optimised‑out uniform. Locations are queried every frame (not cached). |
| 12 | ✅ Fixed (#13) | `GLSLProgram::compileShader` | Compile failure prints but continues to link (link then also fails). Missing file calls `exit(1)`. |
| 13 | ✅ Fixed by the SDL GPU path (#28/#30): `GpuVertex` uploads the colour; legacy `Mesh` is deleted in #40 | `render/3d/Mesh.cpp` `init` | Vertex `color` is never sent to the GPU (attrib 1 is set to color and immediately overwritten by UV). `Sprite::setColor` therefore has no visible effect. |
| 14 | 🗑️ Removed (#32): legacy cameras deleted | `render/camera/FirstPersonCamera.h` | Re‑declares `m_velocity`, `m_isGrounded`, `m_collisionCallback`, **shadowing** the same members of `Camera3D`, and `setCollisionCallback` is non‑virtual. Calling it through a `Camera3D&` sets the wrong callback. |
| 15 | ✅ Fixed (#32): the engine updates every camera's aspect from the swapchain size (`CameraSystem`) | `game/SandboxApp.cpp` `handleEvent` | On resize, only the 3D camera projection updates; (the HUD camera is gone since #9.) `Window::m_width/m_height` are never updated either. |
| 16 | ✅ Fixed (#2) | `render/2d/Animation.cpp` `update` | `_frames.size() - 1` is unsigned → with zero frames it underflows; comparison of `int` with `size_t`. |
| 17 | 🗑️ Removed (#9) | `Texture::loadFromSurface` | For 3‑channel surfaces passes `GL_RGB` without accounting for SDL row pitch (`GL_UNPACK_ALIGNMENT`). TTF blended surfaces are 4‑channel so fine today. |

## Lifetime / resource management

- ~~**SDL init/quit order**~~ – fixed in #17: each module owns its SDL subsystem
  (`SDL_InitSubSystem`/`SDL_QuitSubSystem`), shutdown is reverse‑ordered, and the last module out
  calls `SDL_Quit()` once.
- ~~GL `Texture`/`Mesh`/`GLSLProgram` lifetime bugs (shared ids, double deletes, leaked programs),
  `ResourceManager`~~ – removed with the GL renderer in #40. GPU resources are move‑only RAII
  handles now (`mge_gpu`).

## Structural / debt

- ~~Two namespaces (`Engine` and `MiniGameEngine`) and two naming styles coexist~~ – unified into `mge` with one convention in #10.
- ~~Two delta‑time sources (`Application::m_DeltaTime` and `Time`)~~ – single app‑owned clock with a fixed timestep since #12.
- ~~Unused code: `AudioManager`; `Chunk::getRadius`; `Block::getUVs`~~ – `AudioManager` was replaced by
  `AudioModule` + `mge_synth` (#46); the old `Chunk`/`Block` classes were replaced by `mge_voxel` (#25).
- ~~Unused assets: `sounds/`, `music/`, `levels/`~~ – levels and sounds are 2D test data for
  `examples/platformer2d` (#45). `music/` waits for the M5 sequencer.
- Transparent items are sorted per chunk (#31), not per triangle: overlapping translucent faces inside one chunk can still blend in the wrong order.
- ~~`build/` directory with binaries is committed~~ – fixed in #1.
- `README.md` was a one‑liner before this documentation pass; no tests, no CI.
