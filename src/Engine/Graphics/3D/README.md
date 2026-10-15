# src/Engine/Graphics/3D/

| File | Namespace | Purpose |
|------|-----------|---------|
| `Mesh.h/.cpp` | `MiniGameEngine` | Owns a VAO + VBO + EBO; uploads `Vertex`/index data; draws indexed triangles |

## `Mesh`

The single GPU geometry primitive in the engine. Used directly by the game's `Chunk`
(two meshes per chunk: opaque + transparent) and as the base class of `Sprite`.

| Member | Behaviour |
|--------|-----------|
| `Mesh()` | All ids 0. |
| `virtual ~Mesh()` | Deletes VAO/VBO/EBO if non‑zero. |
| `init(vertices, indices)` | Deletes existing buffers (so it's safe to re‑init), creates new ones, uploads with `GL_STATIC_DRAW`, configures attributes, unbinds VAO. Stores the index count. |
| `virtual draw()` | Binds VAO, `glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, 0)`, unbinds. No‑op if never initialised. Doesn't bind shaders, textures or set uniforms – the caller does. |

Protected members: `_vao`, `_vbo`, `_ebo`, `_indexCount` (accessible to subclasses like `Sprite`).

### Vertex attribute layout (important)

| location | source field | components |
|----------|--------------|------------|
| 0 | `Vertex::position` | 3 × float |
| 1 | `Vertex::uv` | 2 × float |

`init` first points location 1 at `color` (vec4) and then immediately re‑points it at `uv`
(vec2) – the long Spanish comment in `Mesh.cpp` documents this compromise to stay compatible
with the existing shaders. Net effect: **vertex colors never reach the GPU**. To enable them,
add `glVertexAttribPointer(2, 4, …, offsetof(Vertex, color))` and a `layout(location = 2) in vec4`
input in the shaders.

### Caveats
- Not copy‑safe: the implicit copy constructor/assignment copy raw GL ids → double delete.
  Hold meshes by pointer or reference (Chunk uses `Mesh*`).
- `init` must run with a current GL context (after `Window` creation).
- Every `init` call reallocates GPU buffers; `Sprite` calls it on every setter.
