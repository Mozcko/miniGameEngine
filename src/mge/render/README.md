# src/mge/render/  →  target `mge_render`

All OpenGL‑facing code. Requires a live GL 3.3 core context (created by `Window`) before any
of these classes are initialised – in practice, use them from `onInit()` onward.

| Folder | Contents | Doc |
|--------|----------|-----|
| `core/` | `Vertex` (interleaved vertex struct), `Texture` (stb_image → GL texture, runtime tile blitting, SDL_Surface upload), `GLSLProgram` (compile/link shaders, uniforms) | [core/README.md](core/README.md) |
| `3d/` | `Mesh` – owns VAO/VBO/EBO, uploads `Vertex` + index data, draws indexed triangles | [3d/README.md](3d/README.md) |
| `2d/` | `Sprite` (a `Mesh` quad from an atlas tile), `Animation` (time‑based tile index sequencer) (legacy, unused) | [2d/README.md](2d/README.md) |
| `camera/` | `Camera` (abstract), `Camera2D` (ortho), `Camera3D` (perspective free‑fly + collision), `FirstPersonCamera` (gravity/jump) | [camera/README.md](camera/README.md) |

## Class relationships

```
Vertex ──used by──► Mesh ◄──is‑a── Sprite (legacy)                Texture (stb_image)
                     ▲                                   │
                     └── used by game Chunk

mge::Camera (abstract) ◄── Camera2D
mge::Camera3D           ◄── FirstPersonCamera      (Camera3D does NOT derive from Camera)
```

## Cross‑cutting rendering contract

- **Vertex attribute locations** (set in `Mesh::init`): `0 = position (vec3)`, `1 = uv (vec2)`.
  Vertex color is not uploaded. Every shader in `assets/shaders/` follows this.
- **Texture unit**: `Texture::bind()` always uses `GL_TEXTURE0`; samplers are named `myTexture`.
- **Uniform names**: 3D shader uses `model`, `view`, `projection`, `baseColor`; 2D shaders use `P`.
- **Coordinate conventions**: right‑handed, +Y up, camera looks down −Z at yaw −90°. Textures are
  flipped vertically on load, so V = 0 is the bottom of the image; atlas tile rows are counted
  from the top and converted with `1 − row*tileH/texH`.
- **Winding**: front faces are counter‑clockwise (GL default). Chunk faces follow this; `Sprite`
  quads currently do not (see `docs/KNOWN_ISSUES.md`).
