# src/mge/procgen/  →  target `mge_procgen`

**Procedural generation on the CPU**: it depends only on `mge_core` + GLM, so it's **headless**,
is built in every configuration (including `-DMGE_BUILD_MODULES=OFF`), and is tested in the
core‑only CI job. Programs that link it see `MGE_HAS_PROCGEN`, and `<mge/mge.h>` then includes it.

| File | Contents |
|------|----------|
| `MeshData.h/.cpp` | `MeshData` (CPU mesh) and `writeObj` (Wavefront OBJ export) |
| `MeshOps.h/.cpp` | transform, append/merge, weld, flip, recompute normals, polygon triangulation, extrude, lathe, sweep |
| `detail/MeshBuilding.h/.cpp` | Internal helpers (oriented triangles, `revolve`), not public API |
| `Primitives.h/.cpp` | `mge::primitives::` plane, box, uvSphere, icosphere, cylinder, cone, torus, capsule |

Planned in M2: noise (#22), SDF + surface nets (#23).

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

**GPU:** `mge::Mesh::init(const MeshData&)` (`mge_render`) uploads it. The legacy OpenGL renderer
uses position, colour and UV, and ignores normals until M3.

**Gallery:** `examples/procgen_gallery` exports every generated mesh to `<build>/gallery/*.obj`.

Tests: `tests/procgen/` → `mge_procgen_tests`.
