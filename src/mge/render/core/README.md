# src/mge/render/core/

Low‑level GPU building blocks. Namespace: `mge`.

| File | Purpose |
|------|---------|
| `Vertex.h` | Interleaved vertex struct shared by all meshes |
| `Texture.h/.cpp` | Image → GL texture (stb_image), runtime atlas tile copy, SDL_Surface upload |
| `GLSLProgram.h/.cpp` | Load, compile and link a vertex+fragment shader pair; uniform lookup |

---

## `Vertex` (header‑only)

```cpp
struct Vertex {
    glm::vec3 position;   // offset 0   (Z = 0 for 2D)
    glm::vec4 color;      // offset 12  (NOT uploaded to the GPU by Mesh – see 3D/README.md)
    glm::vec2 uv;         // offset 28
    void setPosition(x,y,z); void setUV(u,v); void setColor(r,g,b,a);
};                        // sizeof == 36
```
Aggregate‑initialisable in this order: `Vertex{{x,y,z},{r,g,b,a},{u,v}}` (Chunk does this).
Field order matters – `Mesh::init` uses `offsetof`.

---

## `Texture`

A **lightweight, copyable handle** around a GL texture id plus a CPU copy of the pixels.

| Member | Behaviour |
|--------|-----------|
| `load(path)` | Deletes any previous id held by *this* object; `stbi_set_flip_vertically_on_load(true)`; loads with native channel count; keeps the pixels in `m_originalPixels`; creates `GL_TEXTURE_2D` with **`GL_NEAREST`** min/mag filter, `CLAMP_TO_EDGE`, format `GL_RGB` or `GL_RGBA` (3/4 channels only), generates mipmaps. Logs and returns on failure (id stays 0). |
| `bind()` | `glActiveTexture(GL_TEXTURE0)` + bind. Always unit 0. |
| `unbind()` | Binds 0. |
| `updateTile(dest, src, tileW, tileH)` | Copies atlas tile `src` (from the **original** pixels) into tile `dest` on the GPU with `glTexSubImage2D`. Tile indices are row‑major from the image's top‑left. Used for animated water/lava. Leaves the texture bound. |
| `destroy()` | `glDeleteTextures` + id = 0. **Must be called explicitly**; the destructor intentionally does nothing so cached copies (ResourceManager) stay valid. |
| `getWidth/getHeight/getID` | Accessors. |

Memory note: `m_originalPixels` keeps a full RAM copy (1 MB for the 512×512 RGBA tileset).

`STB_IMAGE_IMPLEMENTATION` is defined at the top of `Texture.cpp` – the only place it may be.

---

## `GLSLProgram`

Typical usage (mirrors `SandboxApp::onInit`):
```cpp
GLSLProgram s;
s.compileShaders("../assets/shaders/basic3d.vert", "../assets/shaders/basic3d.frag");
s.addAttribute("vertexPosition");   // → location 0
s.addAttribute("vertexUV");         // → location 1
s.linkShaders();
...
s.use();
glUniformMatrix4fv(s.getUniformLocation("view"), 1, GL_FALSE, &view[0][0]);
...
s.unuse();
```

| Method | Behaviour |
|--------|-----------|
| `compileShaders(vsPath, fsPath)` | Creates program + both shader objects, reads files, compiles. Unreadable file → `perror` + **`exit(1)`**. Compile error → logs info log, deletes the shader, continues. |
| `addAttribute(name)` | `glBindAttribLocation(program, n++, name)`. Must be called before `linkShaders`. Redundant with the shaders' `layout(location=…)` qualifiers (those win). Also sets how many attrib arrays `use()` enables. |
| `linkShaders()` | Attaches, links; on failure logs and deletes program+shaders; on success detaches/deletes shader objects. |
| `use()` / `unuse()` | `glUseProgram(id / 0)` and enable/disable attrib arrays `0..n-1` (see KNOWN_ISSUES #9). |
| `getUniformLocation(name)` | `glGetUniformLocation`; logs `Uniform X no encontrado` and returns −1 if missing. Not cached – called every frame by the game. |

The destructor does not delete the GL program. No uniform setter helpers exist; call `glUniform*`
directly with the returned location.

---

## `mge::ResourceManager`

All‑static texture cache: `std::map<std::string, Texture>`.

| Method | Behaviour |
|--------|-----------|
| `getTexture(path)` | Returns cached `Texture` copy, or loads it with `Texture::load`, caches it, returns it. Logs `[ResourceManager] Cargando nueva textura desde disco: …`. Load failures are cached too (texture id 0). |
| `clear()` | Clears the map. **Does not free GPU memory** – `Texture`'s destructor intentionally doesn't call `glDeleteTextures`; you'd need to call `destroy()` on each entry first. |

Returned `Texture`s are cheap value copies sharing one GL id (see Graphics/Core/README.md).
Not used by SandboxApp (it owns its `Texture m_tilesetTexture` directly).
