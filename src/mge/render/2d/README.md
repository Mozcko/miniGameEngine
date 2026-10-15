# src/mge/render/2d/

2D helpers. Namespace: `MiniGameEngine`. These are largely leftovers from an earlier 2D
platformer stage of the project (see `assets/levels/`); today only `GUILabel` (and the `Sprite`
it contains) is used, for the HUD in `SandboxApp`.

| File | Purpose | Used today? |
|------|---------|-------------|
| `Sprite.h/.cpp` | Textured quad that **is a** `Mesh`; picks a tile from a texture atlas | Indirectly (via GUILabel) |
| `Animation.h/.cpp` | Time‑based sequence of atlas tile indices | No |
| `GUILabel.h/.cpp` | Renders a string with SDL_ttf into a texture and draws it on a Sprite | Yes (FPS + info labels) |

---

## `Sprite : public Mesh`

| Method | Behaviour |
|--------|-----------|
| `init(x, y, w, h)` | Whole texture on a w×h quad (calls `initFromAtlas` with tile = whole image). |
| `initFromAtlas(x, y, w, h, texW, texH, tileW, tileH, tileIndex)` | Computes UVs for tile `tileIndex` (row‑major from **top‑left**: `col = idx % (texW/tileW)`, `row = idx / cols`, `v = 1 − row*tileH/texH`), applies horizontal flip, builds 4 vertices (BL, TL, TR, BR) at z = 0 and indices `0,1,2, 2,3,0`, then `Mesh::init`. |
| `setPosition(x, y)` | Stores and **rebuilds the whole mesh** (positions are baked into vertices; no model matrix). |
| `setFlipX(bool)` | Swaps u_min/u_max; rebuilds only if changed. |
| `setColor(r,g,b,a)` | Stores tint and rebuilds – but color isn't uploaded by `Mesh`, so no visible effect today. |
| `draw()` | Inherited from `Mesh`. Bind a shader (`texture.vert/frag`) and texture first. |

Coordinates are whatever the projection matrix (`P` uniform) defines. Winding of the quad is
clockwise → culled if `GL_CULL_FACE` is enabled.

---

## `Animation`

```cpp
Animation walk(0.1f /*seconds per frame*/, true /*loop*/);
walk.addFrame(4); walk.addFrame(5); walk.addFrame(6);
walk.update(dt);
sprite.initFromAtlas(..., walk.getCurrentTile());
```
| Method | Behaviour |
|--------|-----------|
| `Animation(speed = 0.1, loop = true)` | `speed` = seconds per frame. |
| `addFrame(tileIndex)` | Appends an atlas tile index. |
| `update(dt)` | Accumulates time; advances at most **one** frame per call (subtracts `speed`, keeps remainder). Non‑looping animations stop on the last frame. |
| `getCurrentTile()` | Current tile index, or 0 if empty. |

Not to be confused with the water/lava animation in SandboxApp, which uses `Texture::updateTile`.

---

## `GUILabel`

On‑screen text. Owns a `TTF_Font*`, a `Texture` and a `Sprite`.

| Method | Behaviour |
|--------|-----------|
| `init(fontPath, pointSize)` | `TTF_OpenFont`. Requires `TTF_Init()` (done by Application). |
| `setText(text, color = white)` | No‑op if text and color are unchanged. Otherwise `TTF_RenderText_Blended` → `Texture::loadFromSurface` → `Sprite::init(0, 0, surfaceW, surfaceH)`. Color is `glm::vec4` in 0..1. |
| `setPosition(x, y)` | Stored; applied at draw time. |
| `setScale(s)` | Stored but **not applied** (no model matrix in the 2D shader). |
| `draw(shader, projection)` | `shader.use()`, uploads `projection` to uniform `P`, `_sprite.setPosition(...)` (rebuilds GPU buffers every frame), binds texture, draws, unbinds. Skips if text is empty. |
| `~GUILabel()` | `TTF_CloseFont`. The GL texture isn't destroyed. |

Expected shader: `assets/shaders/texture.vert/.frag`. Positions are meant to be screen pixels
(origin bottom‑left), which requires a pixel‑space ortho projection – the current `Camera2D` isn't
one (see `docs/KNOWN_ISSUES.md` #1–#2).
