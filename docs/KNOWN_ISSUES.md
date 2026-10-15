# Known issues and technical debt

Found during a full read of the code (state at commit `09a3bfd`, "version1.0 with 2d and 3d
assets"). Severity is a rough guide. Line numbers may drift: search for the quoted code.

## Bugs / likely visible problems

| # | Severity | Where | Issue |
|---|----------|-------|-------|
| 0 | ✅ Fixed (#5) | `Game/Chunk.cpp` `generate`, pass 2 | **Trees and roses never spawn.** Both passes call `srand(seed)` with the same seed. In pass 1, a grass column (height > 6) consumes its first `rand()%100` for the gravel test and stays Grass only if it is ≥ 5. Pass 2 re‑seeds and its first `rand()%100` is that **same value**, so `r < 2` (tree) and `r < 4` (rose) can never be true. Only `r == 5` → Flower (~1 %) remains. A headless run over 1,600 chunks produced 0 Log, 0 Leaves, 0 Rose and 1,422 Flower blocks. Fix: use a different seed for pass 2 (e.g. `seed ^ 0x9E3779B9`) or a per‑column hash instead of global `srand/rand`. |
| 18 | Medium | `Graphics/Core/Texture.cpp` `loadFromSurface` + `Sprite.cpp` UVs | **Text renders upside down** once the HUD is visible. SDL surfaces are top‑row‑first and aren't flipped on upload, but `Sprite` maps v = 1 to the quad's top. |
| 19 | ✅ Fixed (#6) | `Game/SandboxApp.cpp` `OnRender` | **FPS label turns white:** `setText("FPS: …")` uses the default white color, so the yellow from `OnInit` is lost on the first refresh. Pass the color every time. |
| 20 | Medium (latent) | `Graphics/Core/Texture.cpp` `load` | **Buffer over‑read** for 1‑ or 2‑channel images (grayscale or grayscale+alpha PNGs): format is `GL_RGB` for anything that isn't 4 channels, so GL reads 3 bytes per pixel from a smaller buffer. Fix: `stbi_load(..., 4)` and always use `GL_RGBA`. |
| 21 | Medium (latent) | `Game/SandboxApp.cpp` `OnRender`, `basic3d.frag` | Cut‑out blocks (Leaves, flowers) are drawn in the transparent pass with depth writes off and no sorting, so a farther leaf can be drawn over a nearer one. This is hidden today because no trees spawn (#0). Fix: `if (color.a < 0.5) discard;` in the shader and draw cut‑outs in the opaque pass; keep only water/lava/glass in the blended pass, sorted back to front. |
| 22 | Low–Medium | `Window/Window.cpp` `init` | No depth buffer size is requested (`SDL_GL_DEPTH_SIZE`). SDL's default is 16 bits, and with near = 0.1 / far = 1000 precision gets coarse at distance, which can cause z‑fighting. Request 24 bits and/or raise the near plane. Many drivers already return 24, so it may not show on this machine. |
| 23 | Low | `Window/Window.cpp` | No `SDL_GL_SetSwapInterval`, so the frame rate is uncapped unless the driver forces vsync (100 % GPU/CPU use). |
| 24 | ✅ Fixed (#2) | `CMakeLists.txt` | No default `CMAKE_BUILD_TYPE`, so a plain `cmake -S . -B build` compiles **without optimisation** (no `-O2`) and makes chunk generation and meshing slow. Add a Release default or pass `-DCMAKE_BUILD_TYPE=Release`. No warning flags are enabled either. |
| 25 | Low | `Graphics/Core/GLSLProgram.cpp` `linkShaders` | On link failure the program is deleted but `_programID` keeps the dead id, so later `use()` calls generate GL errors instead of failing clearly. |
| 26 | Low | `Core/Application.cpp` ctor | If `SDL_Init` fails it returns before creating `m_Window`; `Run()` then dereferences the null window. |
| 27 | Low | `Game/Chunk.cpp` `generate` | `srand(worldX * 8921 + worldZ * 33771)` overflows `int` (undefined behaviour) for coordinates beyond ~±60 000 blocks, and the global `srand` reseeds `rand()` for any other code that uses it. |
| 1 | High | `Graphics/Camera/Camera2D.cpp` `updateProjection` + `Game/SandboxApp.cpp` HUD | `Camera2D` builds `ortho(-aspect*zoom, aspect*zoom, -zoom, zoom)` (≈ −1.78..1.78 × −1..1) but `GUILabel`s are positioned and sized in **pixels** (`setPosition(10, 680)`, sprite size = text surface size). Labels land far outside the view. A pixel HUD needs `ortho(0, width, 0, height)`. |
| 2 | High | `Graphics/2D/Sprite.cpp` `initFromAtlas` | Quad indices `0,1,2 / 2,3,0` over BL, TL, TR, BR are **clockwise**. With `GL_CULL_FACE` enabled by SandboxApp, sprites/labels are culled. Either reverse winding or disable culling for the HUD pass. |
| 3 | Medium | `Graphics/Camera/Camera3D.cpp` `isSphereVisible` | Always returns `true` – frustum culling is a stub. Chunk loading and drawing are therefore distance‑only. |
| 4 | Medium | `Game/SandboxApp.cpp` `OnUpdate` | Chunks are created synchronously and **never unloaded**; memory grows as you explore. A new chunk causes a frame hitch. |
| 5 | Medium | `Game/Chunk.cpp` `getNeighborBlock` | Faces on chunk borders are culled using a procedural *guess* (water ≤ 5 else "stone") instead of the neighbour chunk's real blocks: trees, lava, flowers and sand across borders are ignored, so faces may be missing or extra at seams. Water level `5` is duplicated here instead of using `WATER_LEVEL`. |
| 6 | Medium | `Game/Chunk.cpp` `placeTree` | Leaves outside the chunk are simply dropped → trees cut at chunk borders. |
| 7 | Medium | `Graphics/2D/GUILabel.cpp` `draw` | `_sprite.setPosition()` is called every draw, which **re‑creates the VAO/VBO/EBO** every frame. Also `_scale` and the computed `model` matrix are unused. |
| 8 | ✅ Fixed (#6) | `Game/SandboxApp.cpp` `OnRender` | FPS = `1 / Time::getDeltaTime()`; when paused dt = 0 → division by zero, float→int overflow (UB). Use unscaled dt. |
| 9 | Low | `Graphics/Core/GLSLProgram.cpp` `use/unuse` | Calls `glEnable/DisableVertexAttribArray` with no VAO bound (core profile → `GL_INVALID_OPERATION`), and it would modify whatever VAO is bound. Attribute state already lives in each Mesh's VAO, so this is unnecessary. |
| 10 | Low | `GLSLProgram::addAttribute` | `glBindAttribLocation` is overridden by `layout(location=…)` in the shaders; harmless but misleading. |
| 11 | Low | `GLSLProgram::getUniformLocation` | (`GL_INVALID_INDEX` comparison fixed in #2.) Prints for every lookup of an optimised‑out uniform. Locations are queried every frame (not cached). |
| 12 | Low | `GLSLProgram::compileShader` | Compile failure prints but continues to link (link then also fails). Missing file calls `exit(1)`. |
| 13 | Low | `Graphics/3D/Mesh.cpp` `init` | Vertex `color` is never sent to the GPU (attrib 1 is set to color and immediately overwritten by UV). `Sprite::setColor` therefore has no visible effect. |
| 14 | Low | `Graphics/Camera/FirstPersonCamera.h` | Re‑declares `m_Velocity`, `m_IsGrounded`, `m_CollisionCallback`, **shadowing** the same members of `Camera3D`, and `setCollisionCallback` is non‑virtual. Calling it through a `Camera3D&` sets the wrong callback. |
| 15 | Low | `Game/SandboxApp.cpp` `OnEvent` | On resize, only the 3D camera projection updates; `_guiCamera` is not updated. `Window::_width/_height` are never updated either. |
| 16 | ✅ Fixed (#2) | `Graphics/2D/Animation.cpp` `update` | `_frames.size() - 1` is unsigned → with zero frames it underflows; comparison of `int` with `size_t`. |
| 17 | Low | `Texture::loadFromSurface` | For 3‑channel surfaces passes `GL_RGB` without accounting for SDL row pitch (`GL_UNPACK_ALIGNMENT`). TTF blended surfaces are 4‑channel so fine today. |

## Lifetime / resource management

- **SDL init/quit order**: `SDL_Quit()` is called by both `~Application` and `~Window`; Window's
  `init()` also calls `SDL_Init(SDL_INIT_VIDEO)` again. `~Application` runs `SDL_Quit/TTF_Quit`
  *before* `m_Window` (and the GL context) is destroyed. `AudioManager::init` runs before `SDL_Init`.
- **Texture** copies share the same GL id and the destructor doesn't free it: textures leak
  unless `destroy()` is called; calling `destroy()`/`load()` on one copy invalidates the others.
- **Mesh** has a user‑defined destructor but default copy ctor/assignment → copying a Mesh (or
  Sprite, GUILabel) double‑deletes GL buffers. Should be made non‑copyable or movable.
- **GLSLProgram** never deletes its program object.
- **GUILabel** default copy would double‑close `TTF_Font*`.
- `ResourceManager::clear()` doesn't call `Texture::destroy()` (its comment assumes the destructor
  frees GPU memory; it doesn't).

## Structural / debt

- Two namespaces (`Engine` and `MiniGameEngine`) and two naming styles coexist.
- Two delta‑time sources (`Application::m_DeltaTime` and `Time`), and the game ignores the first.
- `Window::handleEvents()` / `isClosed()` / `close()` are unused (Application polls events itself).
- Unused code: `ResourceManager`, `Animation`, `FirstPersonCamera`, `AudioManager` (initialised
  but nothing is played), `Camera2D::update` view matrix (GUI uses projection only),
  `Chunk::getRadius` hard‑coded 14 (actual half‑diagonal of 16³ is ≈ 13.86, fine).
- Unused assets: `color.vert/.frag`, `sounds/`, `music/`, `levels/` (no level parser exists).
- Asset paths are hard‑coded relative to `build/`.
- `Block::getUVs` hard‑codes atlas size 512 / tile 32.
- `Sprite` inherits from `Mesh` and rebuilds GPU buffers on any setter instead of using a model
  matrix (comments plan this for "v2.0").
- No transparent sorting – overlapping transparent surfaces may blend incorrectly.
- ~~`build/` directory with binaries is committed~~ – fixed in #1.
- `README.md` was a one‑liner before this documentation pass; no tests, no CI.
