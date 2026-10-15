# assets/textures/

| File | Format | Used by |
|------|--------|---------|
| `Minecraft-like_3d_tileset.png` | 512×512 PNG, 8‑bit RGBA | `SandboxApp::m_tilesetTexture` (all block faces) |

## Atlas layout
- 32×32 px tiles → **16 columns × 16 rows = 256 tiles**, indices `0..255`.
- Index = `row * 16 + col`, counted from the **top‑left** of the image.
- The texture is loaded with a vertical flip (stb_image), so code converts rows with
  `v = 1 − row * 32 / 512`. This math is duplicated in `Block::getUVs` (hard‑coded 512/32),
  `Sprite::initFromAtlas`, and `Texture::updateTile`.
- Filtering is `GL_NEAREST` (crisp pixels); mipmaps are generated, but with `GL_NEAREST` as min
  filter they are not sampled.

## Tiles referenced by code

| Tile | Meaning (from `Block::getUVs`) |
|------|-------------------------------|
| 0 | Grass top |
| 1 | Stone |
| 2 | Dirt (also grass bottom) |
| 3 | Grass side |
| 4 | Wood planks |
| 5 / 6 | Refined stone side / top‑bottom |
| 7 | Brick |
| 8 / 9 / 10 | TNT side / top / bottom |
| 11 | Cobweb |
| 12 | Rose |
| 13 | Flower (daisy) |
| 15 | Sapling |
| 16 | Cobblestone |
| 17 | Coal block |
| 18 | Sand |
| 19 | Gravel |
| 20 / 21 | Log side (bark) / top‑bottom (rings) |
| 49 | Glass |
| 52 | Leaves |
| 205, 206, 207, 222, 223 | Water animation frames; **207** is the tile water blocks sample and gets overwritten at runtime |
| 237, 238, 239, 254, 255 | Lava animation frames; **239** is sampled and overwritten at runtime |
| 255 | Also the "unknown block" fallback in `getUVs` |

If you replace the image, keep it 512×512 with 32 px tiles or update the constants in
`Block::getUVs` and the `updateTile(..., 32, 32)` calls in `SandboxApp::onUpdate`.
