# assets/

Runtime data loaded from disk. Nothing here is compiled or copied by CMake.

## Path rule
Code loads assets through **`mge::assetPath("textures/x.png")`** (`<mge/core/Assets.h>`), which
works from any working directory. The root is logged at startup and can be overridden with
`MGE_ASSET_DIR`. Shaders are not assets: they are compiled and embedded at build time
(`mge_add_shaders`).

## Contents

| Folder | Files | Used by current game? | Doc |
|--------|-------|----------------------|-----|
| `textures/` | `Minecraft-like_3d_tileset.png` (512×512 RGBA atlas) | Yes | [textures/README.md](textures/README.md) |
| `sounds/` | 6 × WAV (8‑bit mono 44.1 kHz) | `examples/platformer2d` (jump, coin, hurt, power_up) | [sounds/README.md](sounds/README.md) |
| `music/` | `time_for_adventure.mp3` | No (music arrives with M5) | [music/README.md](music/README.md) |
| `levels/` | `level1..4.txt` ASCII 2D tile maps | `examples/platformer2d` | [levels/README.md](levels/README.md) |

The sounds, music and levels come from an earlier 2D platformer phase (commit "version1.0 with 2d
and 3d assets"). They are **kept on purpose** as test data for the 2D path: `examples/platformer2d`
loads them, and its smoke and unit tests keep it working (#45).
