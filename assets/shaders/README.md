# assets/shaders/

GLSL `#version 330 core` shaders, loaded at runtime by `MiniGameEngine::GLSLProgram`.
All vertex shaders follow the engine's attribute layout from `Mesh::init`:
**location 0 = position, location 1 = UV**.

| Pair | Inputs | Uniforms | Output | Used by |
|------|--------|----------|--------|---------|
| `basic3d.vert` / `basic3d.frag` | `vec3 vertexPosition` (0), `vec2 vertexUV` (1) | `mat4 model`, `mat4 view`, `mat4 projection`; `sampler2D myTexture`, `vec4 baseColor` | `texture(myTexture, uv) * baseColor` | `SandboxApp::shader3D` → chunks |
| `texture.vert` / `texture.frag` | `vec2 vertexPosition` (0), `vec2 vertexUV` (1) | `mat4 P`; `sampler2D myTexture` | `texture(myTexture, uv)` | `SandboxApp::_guiShader` → `GUILabel` |
| `color.vert` / `color.frag` | `vec2 vertexPosition` (0) | `mat4 P` | constant orange `(1, 0.5, 0, 1)` | Nothing (early test shader) |

### Notes
- The 2D shaders read only x/y of the 3‑float position the Mesh provides (z is dropped).
- No lighting, fog, or vertex colors. Transparency comes purely from the texture alpha plus GL
  blending (`SRC_ALPHA, ONE_MINUS_SRC_ALPHA`); there is no alpha discard, so fully transparent
  texels still write depth in the opaque pass.
- `myTexture` is never explicitly set for the GUI shader – it defaults to unit 0, which is what
  `Texture::bind()` uses.
- To add a shader: create the pair here, then in `OnInit`:
  `compileShaders("../assets/shaders/x.vert", "../assets/shaders/x.frag"); addAttribute(...); linkShaders();`
