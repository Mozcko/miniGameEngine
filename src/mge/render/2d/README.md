# src/mge/render/2d/

2D helpers. Namespace: `mge`. These are leftovers from an earlier 2D platformer
stage of the project (see `assets/levels/`) and are **not used** by the sandbox. `GUILabel` (SDL_ttf
text) was removed in #9; text returns as an embedded bitmap font in M4 (#41), and the rest of this
folder is removed in #45.

| File | Purpose | Used today? |
|------|---------|-------------|
| `Sprite.h/.cpp` | Textured quad that **is a** `Mesh`; picks a tile from a texture atlas | No |
| `Animation.h/.cpp` | Time‑based sequence of atlas tile indices | No |

---

## `Sprite : public Mesh`

| Method | Behaviour |
|--------|-----------|
| `init(x, y, w, h)` | Whole texture on a w×h quad (calls `initFromAtlas` with tile = whole image). |
| `initFromAtlas(x, y, w, h, texW, texH, tileW, tileH, tileIndex)` | Computes UVs for tile `tileIndex` (row‑major from **top‑left**: `col = idx % (texW/tileW)`, `row = idx / cols`, `v = 1 − row*tileH/texH`), applies horizontal flip, builds 4 vertices (BL, TL, TR, BR) at z = 0 and indices `0,1,2, 2,3,0`, then `Mesh::init`. |
| `setPosition(x, y)` | Stores and **rebuilds the whole mesh** (positions are baked into vertices; no model matrix). |
| `setFlipX(bool)` | Swaps uMin/uMax; rebuilds only if changed. |
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
