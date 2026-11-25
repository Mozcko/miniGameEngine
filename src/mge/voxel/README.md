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
| `ChunkStreamer.h/.cpp` | `ChunkStreamer`, `StreamingSettings`, `StreamingStats`, `StreamUpdate`: loads, meshes and unloads chunks around a moving centre, on a `JobSystem` |
| `ChunkSnapshot.h/.cpp` | `ChunkSnapshot`: a copy of a chunk plus a 1‑block border of its neighbours (18³ ids), for meshing on another thread |
| `ChunkMesher.h/.cpp` | `buildChunkMesh(world, coord, options)` → `ChunkMesh {opaque, cutout, transparent}` |
| `ChunkLod.h/.cpp` | `LodChunk`, `downsample(snapshot, registry, lod)`, `buildLodChunkMesh(snapshot, registry, lod, options)` |
| `VoxelRaycast.h/.cpp` | `raycastVoxels(world, ray, maxDistance)` → `optional<VoxelRaycastHit>` (Amanatides–Woo DDA) |

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
| `opaque` | Hides neighbouring faces |
| `layer` | Render pass (`RenderLayer`): `Opaque`, `Cutout` (holes: leaves, plants; alpha test, depth write) or `Translucent` (water, glass; blended). Unset → `Opaque` if `opaque`, else `Translucent` (`renderLayer()`) |
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
  - New chunks start dirty, and so do their 6 loaded face neighbours: their meshes saw air on that
    side.
  - `setBlock` marks the chunk dirty, plus the neighbour across the face when the block sits on a
    border (that neighbour's mesh can see the block).
  - The game re‑meshes `dirtyChunks()` and then calls `clearDirty(coord)`.
- **Other calls:** `unloadChunk`, `chunk(coord)` (nullptr if not loaded), `loadedChunks()`, `chunkCount()`.

## Threads (#56)
Generation and meshing can run on a `JobSystem` without touching `VoxelWorld` from workers:
- **Generate:** `world.generator()->generate(coord, data)` on a worker. Generators must be `const`
  and stateless, which is documented as a requirement. Then `world.insertChunk(coord, data)` on the
  main thread; it dirties neighbours just like `loadChunk`.
- **Mesh:** `ChunkSnapshot::capture(world, coord)` on the main thread (27 chunk lookups, ~11 KB),
  then `buildChunkMesh(snapshot, registry, options)` on a worker. The registry must not change in
  the meantime.
- `buildChunkMesh(world, coord)` is now exactly snapshot + build. Tests check that the parallel
  results are identical to the sequential ones (`tests/voxel/test_parallel.cpp`).

## Streaming – `<mge/voxel/ChunkStreamer.h>` (#57)

```cpp
mge::StreamingSettings settings;
settings.loadRadius = 5;  settings.meshRadius = 4;  settings.unloadMargin = 2;  // columns (Chebyshev X/Z)
settings.minChunkY = 0;   settings.maxChunkY = 3;                             // vertical chunk range
settings.meshOptions = …;                                                     // atlas, AO, greedy
mge::ChunkStreamer streamer(world, settings, &app.jobs());   // nullptr = same logic on the main thread

mge::StreamUpdate u = streamer.update(cameraChunk);          // every frame
for (auto& [coord, mesh] : u.meshes) uploadToGpu(coord, mesh);
for (const auto& coord : u.unloaded) freeGpu(coord);
```

| Behaviour | Details |
|-----------|---------|
| Load | Missing chunks within `loadRadius` (all of `minChunkY..maxChunkY`), **nearest first**; generated on workers (`generator()->generate`), inserted on the main thread |
| Mesh | Dirty chunks within `meshRadius` (< `loadRadius`) whose 4 horizontal neighbours, plus the vertical ones inside the range, have data; snapshot on the main thread, `buildChunkMesh` on a worker |
| Unload | Columns beyond `loadRadius + unloadMargin` (**hysteresis**: crossing a border back and forth doesn't reload). Their storage goes to `VoxelWorld`'s pool (`pooledCount`, `reusedCount`) |
| Budgets | `maxJobsInFlight`; `maxResultsPerUpdate` (generated chunks applied and meshes handed out per update); `maxLoadedChunks` (0 = no limit; evicts the farthest outside `meshRadius` and stops generating when full) |
| Stale results | A generated chunk whose column is now too far, or a mesh for a chunk that was unloaded or edited after the request (the dirty flag is cleared when the job is queued), is **discarded** (`stats().discarded`) |
| Without jobs | The same algorithm on the main thread, with `maxResultsPerUpdate` as the per‑update work limit (tests, headless) |
| `idle()` | Nothing in flight and nothing left to do for the current centre |

The world and its registry must outlive the streamer; its destructor waits for its jobs, and
`Application::run` waits for all jobs before `onShutdown`. Memory stays bounded by
`(2 × (loadRadius + unloadMargin) + 1)² × height` chunks.

## Meshing – `<mge/voxel/ChunkMesher.h>`

```cpp
mge::ChunkMeshOptions options;
options.atlas = {16, 16};          // tiles → UVs
options.greedy = true;             // default: merge equal faces of colour‑only cubes
options.ambientOcclusion = false;  // per‑vertex AO in the colour (aoStrength = 0.5)
options.tintTextured = false;      // textured blocks: white vertex colour (texture as is); true = tint with BlockDesc::color
mge::ChunkMesh mesh = mge::buildChunkMesh(world, {0, 0, 0}, options);
// mesh.opaque / mesh.cutout / mesh.transparent (one per RenderLayer): MeshData in chunk‑local space (0..16);
// mesh.transparentFaceCentroids: one chunk‑local centroid per transparent quad (6 indices), for back‑to‑front
// re‑sorting with mge::sortQuadsBackToFront(indices, centroids, cameraLocal) (#180).
// draw with model = translate(chunkOrigin(coord)). GPU upload: device.uploadMesh(meshData) (mge_gpu).
```

**Face rule:** a cube face is emitted unless the neighbour is an opaque cube, or both blocks are the
same transparent block (water against water).

**Vertices:**
- positions are in chunk‑local space;
- normals are per face;
- colour is `BlockDesc::color` for colour‑only blocks. Textured blocks get white, so the texture
  shows as is, unless `tintTextured`. AO darkens either;
- UVs come from the face's tile, or are (0, 0) for untextured blocks.

**Neighbours** across chunk borders are read from the snapshot's border, and an unloaded neighbour
counts as air. When that neighbour loads, `VoxelWorld` marks this chunk dirty, so re‑meshing the dirty chunks
removes any seam. A game can also load one ring of chunks more than it meshes, which avoids the
re‑mesh; the sandbox does this.

**Greedy meshing** (`greedy`, on by default):
- For each face direction and slice, visible faces of **colour‑only** cubes are merged into maximal
  rectangles of the same block (the classic greedy mask: widen along u, then grow along v).
- **Textured** faces stay one quad each, because an atlas tile can't repeat across a big quad until
  the new renderer (M3).
- On a flat 16×4×16 slab this gives 12 triangles instead of 1 024. On the gallery terrain it roughly
  halves the triangle count.

**Ambient occlusion** (`ambientOcclusion`, off by default):
- Each face corner looks at the 3 blocks around it in the layer in front of the face.
- The vertex colour is darkened by up to `aoStrength` for a fully enclosed corner.
- Quads flip their diagonal when needed to avoid the anisotropy artefact.
- With greedy on, only faces whose 4 corners share the same AO value are merged, so the shading is
  unchanged.
- Diagonal neighbour chunks aren't re‑dirtied, so AO at a chunk corner can be stale until that
  chunk is re‑meshed.

## Terrain LOD – `<mge/voxel/ChunkLod.h>` (#61)

Simplified meshes for distant chunks, drastically reducing triangle counts
and draw overhead.

- **Levels of detail:**
  - **LOD 0**: Original full mesh without simplification (`buildChunkMesh`).
  - **LOD n (1, 2)**: Chunk downsampled by a factor $f = 2^n$ ($16^3 \to 8^3 \to 4^3$ super-blocks of $f^3$ blocks).
- **Top-down visibility rule (downsampling):**
  For each $f^3$ block cell, the block an observer would see from above is chosen: the cell is scanned
  from top to bottom (along the Y axis) and the first **opaque** non-air block is taken along with its
  actual height `topY = y + 1`. Exposed top faces are emitted at `topY` (and side faces of top cells
  or between neighbouring columns of different heights cover the exact height step), avoiding a step
  of up to $f - 1$ blocks at LOD boundaries. If the cell contains no opaque block, air (`kAir`) is chosen.
  Cutout (flowers) and translucent (water, glass) blocks are discarded in the opaque pass at LOD > 0.
  For columns covered by water (non-solid translucent liquid), the top opaque face uses the highest
  opaque block below the water — sitting strictly below the flat water quad (no depth epsilon)
  — and is only omitted if it matches the water height exactly (opaque faces above it, e.g. terrain
  over an underground lake, are kept).
- **Generated mesh:**
  - In chunk-local coordinates (0..16), with the same vertex format and colours as `buildChunkMesh`.
  - Only generates the opaque pass (`mesh.opaque`).
- **Seams between chunks of the same LOD:**
  The neighbouring `ChunkSnapshot` border is downsampled with the same top-down rule, so faces between
  two neighbouring chunks of the same LOD are hidden identically to LOD 0 (no internal faces).
- **Skirts against cracks between different LODs (`lodSkirts`):**
  On the 4 side borders of the chunk ($\pm X, \pm Z$), visible outer faces are extruded downward by
  $f$ blocks (vertical quads of the same colour) to cover any vertical gap with neighbouring chunks
  at a different LOD level. Enabled by default in `ChunkMeshOptions::lodSkirts`.

```cpp
mge::ChunkMeshOptions options;
options.greedy = true;
options.lodSkirts = true;
mge::ChunkMesh lodMesh = mge::buildLodChunkMesh(snapshot, registry, /*lod=*/ 1, options);
// lodMesh.opaque contains the LOD mesh in local 0..16 space
```

## Raycasting – `<mge/voxel/VoxelRaycast.h>` (#51)

Fast voxel grid traversal (Amanatides–Woo DDA) for block picking and line-of-sight:

```cpp
mge::Ray ray(eyePosition, lookDirection);
auto hit = mge::raycastVoxels(world, ray, /*maxDistance=*/ 8.0f);
if (hit) {
    // hit->block: coordinates of the hit block
    // hit->normal: face normal pointing outward (+X, -X, +Y, -Y, +Z, -Z)
    // hit->distance: parametric distance along the ray
    // hit->blockId: BlockId of the hit block
    // hit->point: world hit position (ray.at(hit->distance))
    // hit->face: BlockFace hit
    glm::ivec3 placeAt = hit->block + hit->normal;
}
```

- Traverses unit voxel boundaries in world coordinates; handles chunk boundaries (including negative coordinates) seamlessly.
- If the ray starts inside a solid block, it reports an immediate hit at distance 0 with normal `(0, 0, 0)`.
- Unloaded chunks are treated as air. Returns `std::nullopt` if no solid block is encountered within `maxDistance`.

Tests: `tests/voxel/` → `mge_voxel_tests` (registry, coordinates incl. negatives, world get/set
across chunks, dirty propagation, generator determinism, mesher face rules, atlas UVs, seam‑free
meshing against an independent reference, greedy vs naive area and count, AO, DDA raycasting).
