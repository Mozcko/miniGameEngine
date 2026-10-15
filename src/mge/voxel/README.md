# src/mge/voxel/  →  target `mge_voxel`

**Voxel worlds**: blocks, chunks, a world container, terrain generators and chunk meshing. It
depends only on `mge_core` + `mge_procgen` (for `MeshData`), so it's **headless**: it is built in
every configuration (including `-DMGE_BUILD_MODULES=OFF`) and tested in the core‑only CI job.
Programs that link it see `MGE_HAS_VOXEL`, and `<mge/mge.h>` then includes it.

The game decides *what* blocks exist and *how* terrain looks; the engine stores, queries and meshes
them. The sandbox (`game/`) is the reference user.

| File | Contents |
|------|----------|
| `Block.h/.cpp` | `BlockId` (`kAir = 0`), `BlockFace`, `BlockShape`, `BlockDesc`, `BlockRegistry`, `TextureAtlas`/`UvRect` |
| `Chunk.h/.cpp` | `kChunkSize = 16`, `ChunkCoord` (+ hash), coordinate helpers, `ChunkData` |
| `VoxelWorld.h/.cpp` | `TerrainGenerator` interface, `VoxelWorld` (loaded chunks, world get/set, dirty flags) |
| `ChunkMesher.h/.cpp` | `buildChunkMesh(world, coord, options)` → `ChunkMesh {opaque, transparent}` |

## Blocks – `<mge/voxel/Block.h>`

```cpp
mge::BlockRegistry registry;                  // id 0 = air (not solid, not opaque, no shape)
mge::BlockDesc grass;
grass.name = "grass";
grass.color = {0.36f, 0.62f, 0.25f, 1.0f};   // procedural path: flat vertex colour
grass.setTiles(/*top*/ 0, /*bottom*/ 2, /*sides*/ 3);   // asset path: atlas tiles (optional)
mge::BlockId grassId = registry.add(grass);
```

| `BlockDesc` field | Meaning |
|-------------------|---------|
| `name` | Unique; `registry.find(name)` returns the id |
| `solid` | Collides (`VoxelWorld::isSolid`) |
| `opaque` | Hides neighbouring faces; non‑opaque blocks go to the transparent mesh |
| `shape` | `Cube` (6 culled faces), `Cross` (two diagonal double‑sided quads: plants), `None` (not drawn) |
| `color` | Vertex colour. With a texture it multiplies it (white = texture as is) |
| `tiles[6]` | Atlas tile per `BlockFace` (`Front +Z, Back −Z, Left −X, Right +X, Top +Y, Bottom −Y`), `-1` = untextured. `setTile(t)`, `setTiles(top, bottom, sides)` |

A block can be **colour‑only**, **textured**, or **both** ([ADR 0004](../../../docs/adr/0004-procedural-first-assets-optional.md)).
`TextureAtlas{columns, rows}.tileUv(tile)` gives a tile's UV rectangle (tile 0 is the top‑left;
V points up, matching images loaded with row 0 at the top).

## Chunks and coordinates – `<mge/voxel/Chunk.h>`

- Chunks are **16³** and stack in **all three axes**, so worlds can be tall and deep.
- **Convention:** block `(x, y, z)` occupies the unit cube `[x, x+1) × [y, y+1) × [z, z+1)`.

| Helper | Example |
|--------|---------|
| `floorDiv(a, b)`, `floorMod(a, b)` | Round towards −∞: `floorDiv(-1, 16) = -1`, `floorMod(-1, 16) = 15` |
| `chunkOf(ivec3 block)` | `(-1, 16, 31)` → chunk `{-1, 1, 1}` |
| `localOf(ivec3 block)` | `(-1, 16, 31)` → local `(15, 0, 15)` |
| `chunkOrigin(ChunkCoord)` | First block of the chunk, in world coordinates |
| `blockAt(vec3 point)` | `floor(point)`: the block containing a point (collision, picking) |

`ChunkData` holds the 4096 `BlockId`s (8 KB, x fastest, then z, then y). `get`/`set` take local
coordinates; out of range reads air and writes are ignored. It also has `fill`, `isEmpty` and
`blocks()`.

## World – `<mge/voxel/VoxelWorld.h>`

```cpp
class MyTerrain : public mge::TerrainGenerator {
    void generate(const mge::ChunkCoord& coord, mge::ChunkData& chunk) const override { /* … */ }
};
mge::VoxelWorld world(registry, std::make_unique<MyTerrain>());
world.loadChunk({0, 0, 0});                 // generates once; already loaded → returned as is
world.setBlock({3, 5, -2}, grassId);        // world coordinates
bool solid = world.isSolid({3, 5, -2});
```

- **Generators** must be deterministic and must not look at other chunks. The world is then the
  same whatever order chunks load in.
- **Unloaded chunks read as air**, and `setBlock` on them returns false.
- **Dirty flags:**
  - New chunks start dirty.
  - `setBlock` marks the chunk dirty, plus the neighbour across the face when the block sits on a
    border (that neighbour's mesh can see the block).
  - The game re‑meshes `dirtyChunks()` and then calls `clearDirty(coord)`.
- **Other calls:** `unloadChunk`, `chunk(coord)` (nullptr if not loaded), `loadedChunks()`, `chunkCount()`.

## Meshing – `<mge/voxel/ChunkMesher.h>`

```cpp
mge::ChunkMeshOptions options;
options.atlas = {16, 16};
mge::ChunkMesh mesh = mge::buildChunkMesh(world, {0, 0, 0}, options);
// mesh.opaque / mesh.transparent: MeshData in chunk‑local space (0..16);
// draw with model = translate(chunkOrigin(coord)). GPU upload: Mesh::init(const MeshData&).
```

**Face rule:** a cube face is emitted unless the neighbour is an opaque cube, or both blocks are the
same transparent block (water against water).

**Vertices:**
- positions are in chunk‑local space;
- normals are per face;
- colour is `BlockDesc::color`;
- UVs come from the face's tile, or are (0, 0) for untextured blocks.

**Neighbours** across chunk borders are read from the world, and an unloaded neighbour counts as
air. A game can load one ring of chunks more than it meshes, so borders see real data. The
sandbox does this.

**Planned in M2:**
- #26: re‑mesh neighbours when a chunk loads, greedy meshing for colour‑only cubes, optional AO,
  and trees that cross chunk borders.
- #27: noise terrain.

Tests: `tests/voxel/` → `mge_voxel_tests` (registry, coordinates incl. negatives, world get/set
across chunks, dirty propagation, generator determinism, mesher face rules and atlas UVs).
