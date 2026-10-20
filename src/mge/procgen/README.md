# src/mge/procgen/  →  target `mge_procgen`

**Procedural generation on the CPU**: it depends only on `mge_core` + GLM, so it's **headless**,
is built in every configuration (including `-DMGE_BUILD_MODULES=OFF`), and is tested in the
core‑only CI job. Programs that link it see `MGE_HAS_PROCGEN`, and `<mge/mge.h>` then includes it.

| File | Contents |
|------|----------|
| `MeshData.h/.cpp` | `MeshData` (CPU mesh) and `writeObj` (Wavefront OBJ export) |
| `Image.h/.cpp` | `Image` (RGBA8 CPU image) and `writePng` (dependency‑free PNG writer) |
| `Font.h/.cpp`, `BitmapFont8x8.cpp` | `Font` (glyph atlas + metrics), `Font::builtin()` (embedded 8×8 font), `nextCodepoint` (UTF‑8) |
| `TextureGen.h/.cpp` | `mge::texgen::`: palettes and texture generators (noise, speckle, bricks, planks, ore, stripes, rings, plant, leaves, frame), `tileRect` |
| `MeshOps.h/.cpp` | transform, append/merge, weld, flip, recompute normals, polygon triangulation, extrude, lathe, sweep |
| `Noise.h/.cpp` | `mge::Noise`: value, Perlin, simplex, Worley (2D/3D), fbm, ridged, billow, domain warp, batch sampling |
| `Sdf.h/.cpp` | `mge::sdf::`: signed distance primitives and operators, gradient normals, `surfaceNets` (SDF → MeshData) |
| `detail/MeshBuilding.h/.cpp` | Internal helpers (oriented triangles, `revolve`), not public API |
| `Primitives.h/.cpp` | `mge::primitives::` plane, box, uvSphere, icosphere, cylinder, cone, torus, capsule |


## `MeshData`

```cpp
mge::MeshData mesh;
auto a = mesh.addVertex({-1, 0,  1}, /*normal*/ {0, 1, 0}, /*color*/ {1, 0, 0, 1}, /*uv*/ {0, 0});
auto b = mesh.addVertex({ 1, 0,  1}, {0, 1, 0}, {0, 1, 0, 1}, {1, 0});
auto c = mesh.addVertex({ 1, 0, -1}, {0, 1, 0}, {0, 0, 1, 1}, {1, 1});
auto d = mesh.addVertex({-1, 0, -1}, {0, 1, 0}, {1, 1, 0, 1}, {0, 1});
mesh.addQuad(a, b, c, d);                     // triangles (a,b,c) + (a,c,d), CCW seen from outside
assert(mesh.validate().empty());
mge::writeObj(mesh, "quad.obj");              // look at it in Blender / any OBJ viewer
```

| Member | Notes |
|--------|-------|
| `positions`, `normals`, `colors`, `uvs` | Separate arrays (SoA), **always the same length**: every vertex has every attribute. `addVertex` defaults to normal 0, white, uv 0 |
| `indices` | `uint32`, 3 per triangle, counter‑clockwise seen from outside |
| `addVertex` → index, `addTriangle`, `addQuad` | Building blocks |
| `vertexCount`, `triangleCount`, `empty`, `reserve`, `clear` | |
| `bounds()` | `mge::Aabb` (empty for an empty mesh) |
| `validate()` | Empty string when valid; otherwise the first problem (attribute lengths, index out of range, incomplete triangle) |
| `writeObj(mesh, path)` | `v x y z r g b` (vertex colour extension), `vt`, `vn`, `f a/a/a` (1‑based). Returns false if the file can't be written |

## Primitives – `<mge/procgen/Primitives.h>`

All are centred on the origin, have normals, UVs and an optional colour, and are wound
**counter‑clockwise seen from outside**. Closed shapes are **watertight**: once the seam and pole
duplicates are welded, every edge is shared by exactly two triangles.

| Function | Vertices / triangles | Notes |
|----------|----------------------|-------|
| `plane(w, d, segX = 1, segZ = 1)` | (sx+1)(sz+1) / 2·sx·sz | XZ plane at y = 0, normal +Y (open) |
| `box(size)` | 24 / 12 | Flat faces |
| `uvSphere(r, segments = 32, rings = 16)` | (rings+1)(seg+1) / 2·seg·(rings−1) | Poles are single triangles (no degenerate quads) |
| `icosphere(r, subdivisions = 2)` | 10·4ⁿ+2 / 20·4ⁿ | Shared vertices; spherical UVs have a seam |
| `cylinder(r, h, segments = 32, caps = true)` | 4·seg+6 / 4·seg | y ∈ [−h/2, h/2]; without caps it's open |
| `cone(r, h, segments = 32)` | 3·seg+4 / 2·seg | Base at −h/2, apex at +h/2, slanted side normals |
| `torus(R, r, segments = 48, sides = 24)` | (sides+1)(seg+1) / 2·seg·sides | In the XZ plane |
| `capsule(r, h, segments = 32, rings = 8)` | 2(rings+1)(seg+1) / 4·seg·rings | Total height h + 2r |

Invalid parameters are clamped to safe minimums (e.g. ≥ 3 segments).

How they're built:
- Every round shape is one helper, `revolve()`, that revolves rows (radius, height, normal) around
  Y. A row with radius 0 is a pole and produces triangles, not quads. The torus is a circle
  revolved the same way.
- Winding is never derived by hand: each triangle is checked against its vertices' analytic
  normals and flipped if needed (`addOrientedTriangle`).

Tests (`tests/procgen/test_primitives.cpp`, with helpers in `tests/procgen/MeshChecks.h`):
- exact counts, `validate()`, unit normals;
- **watertight** (each directed edge once, plus its reverse once, after welding);
- **winding matches normals**, outward normals, and bounds that match the parameters.

## Mesh operations – `<mge/procgen/MeshOps.h>`

| Function | Behaviour |
|----------|-----------|
| `transformMesh(mesh, mat4)` | Positions by the matrix, normals by the inverse transpose (renormalised). A **mirror** (det < 0) flips triangle order, so the mesh stays CCW from outside |
| `appendMesh(dst, src)` / `mergeMeshes(a, b)` | Concatenate, offsetting indices |
| `weldVertices(mesh, eps)` | Merges vertices with **all** attributes equal (position, normal, colour, UV), so hard edges and UV seams are kept |
| `flipWinding(mesh)` | Reverses triangles **and** normals |
| `recomputeNormals(mesh, Smooth / Flat)` | Smooth: area‑weighted face normals summed per **position**, so seams don't show. Flat: unwelds; every triangle gets 3 vertices with the face normal |
| `triangulatePolygon(poly)` | Ear clipping for simple polygons (concave OK, CW or CCW input), CCW output |
| `extrude(poly, depth)` | Polygon in XY extruded along Z ∈ [−d/2, d/2]: caps + flat sides; watertight |
| `lathe(profile, segments)` | Profile (x = radius, y = height) revolved around Y. Order it **bottom → top** for outward normals; x = 0 points are poles that close the shape |
| `sweep(profile, path, closedPath, caps)` | Closed profile carried along a 3D path with **parallel‑transport frames** (no sudden twists). Open paths get end caps; closed paths wrap around (e.g. a ring) |

Tests: `tests/procgen/test_mesh_ops.cpp` cover:
- correct ellipsoid normals under non‑uniform scale;
- mirror winding;
- merge offsets;
- weld keeping seams and joining exact duplicates;
- flip;
- Smooth and Flat normals;
- triangulation of a concave L (CW and CCW, area preserved);
- a watertight extrude;
- a lathed cylinder with exact radius, and a closed lathe;
- straight, helix and closed sweeps.

## Noise – `<mge/procgen/Noise.h>`

```cpp
mge::Noise noise(worldSeed);                                     // permutation table from Pcg32(seed)
float h = noise.fbm(mge::NoiseType::Simplex, x, z, {/*octaves*/ 6, /*frequency*/ 0.01f});
float cells = noise.worley(x, z).f1;                             // distance to the nearest feature point
float organic = noise.domainWarp(mge::NoiseType::Perlin, x, z, /*strength*/ 2.0f);
```

| Function | Range | Notes |
|----------|-------|-------|
| `value(x,y[,z])` | [−1, 1] | Smoothly interpolated random lattice values |
| `perlin(x,y[,z])` | ≈[−1, 1] (clamped) | Gradient noise; **exactly 0 at integer points** |
| `simplex(x,y[,z])` | ≈[−1, 1] (clamped) | Gustavson's simplex; fewer directional artefacts (the patent expired in 2022) |
| `worley(x,y[,z])` | `{f1, f2}` distances | One feature point per cell, 3×3(×3) search. f1 < √2 (2D) / √3 (3D) |
| `fbm(type, …, settings)` | [−1, 1] | Sum of octaves, **normalised by the total amplitude** (`octaves`, `frequency`, `lacunarity`, `gain`) |
| `ridged(type, x, y, settings)` | [0, 1] | `(1 − |n|)²` per octave: mountain ridges |
| `billow(type, x, y, settings)` | [−1, 1] | `|n|·2 − 1`: puffy / cloud‑like |
| `domainWarp(type, …, strength, settings)` | [−1, 1] | fbm sampled at coordinates offset by other fbm fields |
| `sample(type, xs, ys[, zs], out, n)` | | Batch loop (same values as single samples) |

Deterministic: the same seed gives the same output on every platform; different seeds use
different permutation tables (Fisher–Yates with `Pcg32`). Worley feature points come from
`hash2i` / `hash3i`.

**Performance** (`bench/noise_bench`, Release, this dev machine, ns per sample): value 2D 29,
Perlin 2D 23, simplex 2D 31, Worley 2D 71, Perlin 3D 49, simplex 3D 51, **Worley 3D 543** (27 cells
× 3 hashes), fbm (5 octaves) 184, domain warp 620. Optimising is M7 work (SIMD, cheaper Worley
hashing).

Tests (`tests/procgen/test_noise.cpp`):
- determinism across seeds;
- range and mean ≈ 0 over 20k samples;
- Perlin lattice zeros;
- continuity (Lipschitz);
- Worley bounds;
- fractal ranges, and that more octaves add fine detail relative to coarse;
- domain warp;
- batch equals single samples.

## Images and texture generators – `<mge/procgen/Image.h>`, `<mge/procgen/TextureGen.h>` (#34)

Textures made by code, the procedural path of [ADR 0004](../../../docs/adr/0004-procedural-first-assets-optional.md).
They are headless and deterministic from a seed.

```cpp
mge::Image image(64, 64);                                     // RGBA8, row 0 at the top
namespace tg = mge::texgen;
tg::bricks(image, {0, 0, 64, 64}, tg::Palette(darkRed, lightRed), mortar, /*rows*/ 6, /*columns*/ 3, /*mortar px*/ 2, seed);
mge::writePng(image, "bricks.png");                           // to look at it
auto texture = mge::Texture2D::fromPixels(device, image.width(), image.height(), image.data());   // mge_graphics
```

**`Image`:**
- `get`/`set` work in colours in [0, 1]; values are clamped, and out of range reads transparent
  and ignores writes;
- `fill`, `data()`, `pixels()`.

**`writePng`:**
- writes a valid PNG with **no dependencies**: zlib "stored" (uncompressed) deflate blocks, CRCs
  and Adler‑32;
- it works in the core‑only build;
- it's meant for inspection, not for small files.

| Generator | Result |
|-----------|--------|
| `Palette({stops})`, `Palette(from, to)` | Colour gradient; `sample(t)` |
| `noise(rect, palette, scale, seed, octaves)` | Value‑noise fbm through a palette (dirt, stone, sand, water, lava) |
| `speckle(rect, color, density, seed)` | Scattered dots |
| `bricks(rect, palette, mortar, rows, columns, mortarPx, seed)` | Offset brick rows with per‑brick tone |
| `planks(rect, palette, boards, seed)` | Boards with grain and dark seams |
| `ore(rect, color, spots, radius, seed)` | Mineral spots |
| `stripes(rect, palette, count, seed)` | Wobbly vertical bands (bark) |
| `rings(rect, palette, spacing, seed)` | Concentric rings (cut log) |
| `plant(rect, stem, bloom, radius, seed)` | Stem, leaves and flower on a **transparent** background (cutout pass) |
| `leaves(rect, palette, holes, seed)` | Noise with 2×2 transparent holes |
| `frame(rect, color, width)` | Border |
| `tileRect(index, columns, tileSize)` | Rect of an atlas tile (tile 0 top‑left) |

`examples/procgen_gallery` writes one sample of each generator to `texture_generators.png`, and
the sandbox builds its whole atlas from them (`game/SandboxMaterials.cpp`).
Tests: `tests/procgen/test_image.cpp`, which includes a PNG round trip with CRC checks.

## Text: fonts – `<mge/procgen/Font.h>` (#41)
A `Font` is a white‑with‑alpha `Image` atlas plus one `Glyph` per ASCII character 32..126
(atlas position, size, offset from the pen and advance, all in font pixels). It is headless; the 2D
renderer uploads the atlas to the GPU.

| API | Notes |
|-----|-------|
| `Font::builtin()` | The embedded 8×8 font: public‑domain IBM VGA glyphs (the same set as `SDL_RenderDebugText`), stored as bits in `BitmapFont8x8.cpp`. Atlas 128×48 (16×6 glyphs), advance 8, line 10 px. `isPixelFont()` → sample it unfiltered and draw at integer scales |
| `glyph(codepoint)` | Anything outside 32..126 (tabs, `ñ`, emoji) falls back to `?` |
| `measure(text, scale)` | Widest line × number of lines, in pixels; `\n` starts a new line |
| `lineHeight()`, `atlas()`, `valid()` | |
| `nextCodepoint(text, i)` | Reads one UTF‑8 character and advances `i`; invalid bytes give U+FFFD |

TTF fonts are optional and live in `mge_graphics` (`loadTtfFont`, stb_truetype), because the
headless modules don't depend on stb. `procgen_gallery` writes the atlas, zoomed ×4, to
`font_builtin.png`. Tests: `tests/procgen/test_font.cpp`.

## SDF + surface nets – `<mge/procgen/Sdf.h>`

A signed distance function returns how far a point is from a surface: negative inside, positive
outside. Combine them to describe shapes with pure math, then mesh them:

```cpp
using namespace mge::sdf;
Sdf blob = opSmoothUnion(sphere(0.8f), opTranslate(box({0.5f, 0.5f, 0.5f}), {0.9f, 0.3f, 0}), /*k*/ 0.3f);
mge::MeshData mesh = surfaceNets(blob, mge::Aabb({-2, -2, -2}, {2, 2, 2}), /*resolution*/ 64);
```

| Group | Functions |
|-------|-----------|
| Primitives | `sphere(r)`, `box(halfExtents)`, `roundBox(half, r)`, `torus(R, r)` (XZ), `capsule(a, b, r)`, `cylinder(r, halfHeight)` (Y), `plane(normal, offset)` |
| Booleans | `opUnion`, `opSubtract` (a minus b), `opIntersect`, and smooth variants `opSmoothUnion/Subtract/Intersect(a, b, k)` (k = blend radius; k = 0 → hard) |
| Transforms | `opTranslate`, `opRotate(quat)`, `opScale(s)` (uniform, keeps exact distances), `opRepeat(period)` (per axis, 0 = no repeat), `opTwist(radiansPerUnit)` (around Y, distance approximate), `opRound(r)` |
| Meshing | `normal(sdf, p)` (central‑difference gradient); `surfaceNets(sdf, bounds, resolution, color)` |

`Sdf` is a `std::function<float(vec3)>`: easy to compose, at the cost of a call per node.

**Surface nets:**
1. Sample the SDF on a grid; `resolution` is the number of cells along the longest axis of
   `bounds`.
2. Place one vertex in every cell the surface crosses, at the average of the edge crossings.
3. Emit one quad per sign‑changing grid edge, joining the 4 cells around it.

Normals come from the SDF gradient and winding uses the oriented‑quad helper. The result is
**watertight** when the shape lies fully inside `bounds`. The algorithm needs no lookup tables,
unlike marching cubes. Sharing SDF definitions with shaders (raymarching, #36) is future work.

Tests (`tests/procgen/test_sdf.cpp`):
- exact primitive distances;
- booleans, including that smooth union never exceeds the hard union;
- every transform (repeat periodicity, exact scaled distances);
- sphere meshes within one cell of the radius;
- watertight, correctly wound sphere, **torus** (genus 1) and smooth blob;
- vertex count grows with resolution;
- empty results when no surface is inside the bounds.

**GPU:** `GpuDevice::uploadMesh(const MeshData&)` (`mge_gpu`) uploads every attribute (position,
normal, colour, UV) as `GpuVertex`. `MeshRenderer` (`mge_graphics`) draws it.

**Gallery:** `examples/procgen_gallery` exports every generated mesh to `<build>/gallery/*.obj`.

Tests: `tests/procgen/` → `mge_procgen_tests`.
