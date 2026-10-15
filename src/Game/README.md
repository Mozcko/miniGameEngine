# src/Game/

The client application: a Minecraft‑style voxel sandbox. Depends on `src/Engine`; nothing
in the engine depends on it except the `Engine::CreateApplication()` definition.

| File | Contents |
|------|----------|
| `SandboxApp.cpp` | `SandboxApp : Engine::Application` (game loop callbacks), `ChunkKey`, and `Engine::CreateApplication()` |
| `Chunk.h/.cpp` | `Chunk` – 16×16×16 block storage, terrain generation, mesh building, drawing. Constants `CHUNK_SIZE = 16`, `CHUNK_HEIGHT = 16` |
| `Block.h/.cpp` | `BlockType` enum, `UVRect`, `Block` (solid/opaque rules, per‑face atlas UVs) |

Block and Chunk live in namespace `MiniGameEngine` (same as engine classes).

---

## `SandboxApp`

### Members
| Member | Type | Role |
|--------|------|------|
| `shader3D` | `GLSLProgram` | `basic3d.vert/frag` – world rendering |
| `camera` | `Engine::Camera3D` | Player view, created 1280×720, starts at (0, 25, 0) |
| `chunkMap` | `std::map<ChunkKey, Chunk*>` | All loaded chunks, owned (deleted in destructor) |
| `tilesetTexture` | `Texture` | `Minecraft-like_3d_tileset.png` |
| `_guiShader` | `GLSLProgram` | `texture.vert/frag` – HUD |
| `_guiCamera` | `Engine::Camera2D` | HUD projection |
| `_fpsLabel`, `_infoLabel` | `GUILabel` | Yellow "FPS: N" (PixelOperator8‑Bold 24) and white "MiniGameEngine v1.0 - WASD to Move" (PixelOperator8 16) |
| `waterFrames` / `lavaFrames` | `vector<int>` | `{205,206,207,222,223}` / `{237,238,239,254,255}` atlas tiles |
| `waterTarget` / `lavaTarget` | `int` | Tiles 207 / 239 – overwritten each animation step |
| `animTimer`, `currentAnimFrame` | | 0.2 s per animation frame, on unscaled time |
| `isPaused`, `mouseCaptured` | `bool` | Pause state; `mouseCaptured` is always true |

### `OnInit`
1. Compile/link `shader3D`; set the camera position; install the **collision callback**: convert the
   AABB to block coordinates with `floor(v + 0.5)` and return true if any block in range
   `isSolid()` (via `getGlobalBlock`).
2. Load the tileset texture.
3. GL state: depth test on, back‑face culling on, alpha blending.
4. `Time::init()`, lock the cursor.
5. Compile/link `_guiShader`, initialise both labels.

### `OnEvent`
| Event | Action |
|-------|--------|
| Window resized | `glViewport` + `camera.updateProjection` (HUD camera not updated) |
| Focus gained (not paused) | Re‑lock cursor |
| Mouse button down (not paused) | Re‑lock cursor |
| `Esc` | `m_Running = false` |
| `P` | Toggle pause: time scale 0/1, cursor unlock/lock |

### `OnUpdate(dt)` (the `dt` argument is ignored)
1. `Time::update()`; `deltaTime = Time::getDeltaTime()`.
2. Every 0.2 s of unscaled time: `tilesetTexture.updateTile(207, waterFrames[i % 5], 32, 32)` and
   the same for lava into 239.
3. Chunk streaming: camera chunk `(floor(x/16), floor(z/16))`; for every chunk within
   `renderDist = 4` (9×9 = 81 chunks) that passes `isSphereVisible` (always true) and isn't
   loaded → `new Chunk(cx, cz)` (generation) then `chunk->buildMesh()` (GPU upload), both synchronously.
4. `camera.update(deltaTime)`.

### `OnRender`
Sky clear → opaque pass over all chunks → transparent pass with `glDepthMask(GL_FALSE)` →
HUD with depth test off (FPS text refreshed every 60 frames). See `docs/ARCHITECTURE.md` §6.

### `getGlobalBlock(x, y, z)`
World block coords → chunk key via `floor(x / 16)`, local coords via `%` with negative fix‑up.
Unloaded chunk → `Air`. `y` is passed through; `Chunk::getBlock` returns Air outside 0..15.

### `ChunkKey`
`{int x, z}` with `operator<` (via `std::tie`) for use as a `std::map` key.
(`<tuple>` is not included explicitly; it arrives transitively.)

---

## `Chunk`

World‑space origin `(cx*16, 0, cz*16)`; block `(x,y,z)` is a unit cube centred at
`origin + (x, y, z)`. Storage: `Block _blocks[16][16][16]` indexed `[x][y][z]`.

| Method | Behaviour |
|--------|-----------|
| `Chunk(cx, cz)` | `generate()` only (CPU data, no GPU) – testable headless. Call `buildMesh()` before drawing. |
| `~Chunk()` | Deletes both meshes. |
| `generate()` | Pass 1 terrain, pass 2 decoration (below). |
| `buildMesh()` | Builds `_meshOpaque` and `_meshTransparent` (below). Calling it twice leaks the old meshes. |
| `drawOpaque(shader)` / `drawTransparent(shader)` | Sets uniform `model = translate(getPosition())` and draws. Shader must already be in use. |
| `getBlock(x,y,z)` | Local coords; Air when out of range. |
| `getPosition()`, `getCenter()`, `getRadius()` (14) | For placement / culling. |
| `getTerrainHeight(wx, wz)` (static, private) | `int((sin(wx*0.2) + cos(wz*0.2)) * 3 + 6)` clamped to [1, 15]. |
| `placeTree(x,y,z)` (private) | Trunk of 4–5 `Log`; diamond‑ish canopy of `Leaves` (Manhattan dist < 4, radius 2, 4 layers from `y+h-2` to `y+h+1`) only inside this chunk and only into air. |
| `getNeighborBlock(x,y,z)` (private) | Inside chunk → real block; outside in XZ → *procedural guess* (above height: water if y ≤ 5 else air; else Stone). |

### Generation rules (`WATER_LEVEL = 5`)
Per column, seeded with `srand(wx*8921 + wz*33771)` (deterministic; the same seed is re‑applied
in pass 2):
- `y > height`: Water if `y ≤ 5`, else Air.
- `y == height`: Lava if lava pool (`rand()%200 == 0` and height > 5), else surface:
  Sand if `height ≤ 6`, else Gravel with 5 % chance, else Grass.
- `height-3 < y < height`: Dirt (2 layers). Below: Stone.
- Pass 2, on Grass above water: 2 % tree, 2 % Rose, 2 % Flower (one `rand()%100` roll).

Pass 2 seeds with `seed ^ 0x5bd1e995`. Re‑using pass 1's seed made the decoration roll repeat the
gravel roll, so trees and roses never spawned (fixed in #5). A regression test in
`tests/test_chunk.cpp` checks that each decoration lands at 1–3 % of grass columns; the measured
rate is ≈ 1.9 %.

### Meshing rules
- Skip Air. Non‑opaque blocks go to the transparent mesh, others to the opaque mesh.
- **Rose, Flower, Sapling, Cobweb**: "cross" model – two diagonal double‑sided quads using face‑0 UVs.
- Other blocks: for each of the 6 faces, emit a quad if `shouldDrawFace(neighbor)`:
  neighbour is Air → draw; neighbour non‑opaque → draw if we're opaque or of a different type
  (so water–water faces are hidden, water–glass faces drawn); neighbour opaque → hide.
- Face vertex order is counter‑clockwise seen from outside; the Back face uses the
  `invIndices` template `{1,0,3, 3,2,1}` because its vertex list is mirrored.
- Every vertex has white color (unused by the GPU, see Mesh).

---

## `Block` / `BlockType`

`Block` is a 1‑field value type (`BlockType _type`, default Air).

| Rule | Types |
|------|-------|
| `isActive()` false | Air |
| **Not solid** (walk through) | Air, Rose, Flower, Sapling, Cobweb, Water, Lava |
| **Not opaque** (transparent mesh, doesn't hide neighbours) | the non‑solid list + Glass, Leaves |

### Face indices
`0 Front (+Z) · 1 Back (−Z) · 2 Left (−X) · 3 Right (+X) · 4 Top (+Y) · 5 Bottom (−Y)`

### Atlas tile per type (`Block::getUVs`)
Atlas 512×512, 32 px tiles, 16 columns; index = row*16 + col from the top‑left.

| BlockType | Tile(s) |
|-----------|---------|
| Grass | top 0, bottom 2, sides 3 |
| Dirt | 2 |
| Stone | 1 |
| Wood (planks) | 4 |
| RefinedStone | top/bottom 6, sides 5 |
| Brick | 7 |
| TNT | top 9, bottom 10, sides 8 |
| Cobweb | 11 |
| Rose | 12 |
| Flower | 13 |
| Sapling | 15 |
| Cobblestone | 16 |
| Coal | 17 |
| Sand | 18 |
| Gravel | 19 |
| Log | top/bottom 21, sides 20 |
| Glass | 49 |
| Leaves | 52 |
| Water | 207 (animated: 205, 206, 207, 222, 223) |
| Lava | 239 (animated: 237, 238, 239, 254, 255) |
| anything else | 255 (debug) |

Types defined but never generated by terrain: Wood, RefinedStone, Brick, TNT, Cobweb, Sapling,
Cobblestone, Coal, Glass.

### Adding a new block type
1. Append to `enum class BlockType` in `Block.h`.
2. Add a `case` in `Block::getUVs` (and in `isSolid` / `isOpaque` if it's not a solid opaque cube).
3. If it should render as a cross/billboard, add it to the cross‑model check in `Chunk::buildMesh`.
4. Place it in `Chunk::generate` (or via future editing code).
