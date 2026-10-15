# src/mge/procgen/  →  target `mge_procgen`

**Procedural generation on the CPU**: it depends only on `mge_core` + GLM, so it's **headless**,
is built in every configuration (including `-DMGE_BUILD_MODULES=OFF`), and is tested in the
core‑only CI job. Programs that link it see `MGE_HAS_PROCGEN`, and `<mge/mge.h>` then includes it.

| File | Contents |
|------|----------|
| `MeshData.h/.cpp` | `MeshData` (CPU mesh) and `writeObj` (Wavefront OBJ export) |

Planned in M2: primitives (#20), mesh operations (#21), noise (#22), SDF + surface nets (#23).

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

**GPU:** `mge::Mesh::init(const MeshData&)` (`mge_render`) uploads it. The legacy OpenGL renderer
uses position, colour and UV, and ignores normals until M3.

**Gallery:** `examples/procgen_gallery` exports every generated mesh to `<build>/gallery/*.obj`.

Tests: `tests/procgen/` → `mge_procgen_tests`.
