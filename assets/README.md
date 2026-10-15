# assets/

Runtime data loaded from disk. Nothing here is compiled or copied by CMake.

## Path rule
All code references assets as **`"../assets/<subdir>/<file>"`**, i.e. relative to the process
working directory, which must be `build/` (a sibling of `assets/`). There is no asset root
setting.

## Contents

| Folder | Files | Used by current game? | Doc |
|--------|-------|----------------------|-----|
| `shaders/` | `basic3d.*`, `texture.*`, `color.*` (GLSL 330 core) | basic3d + texture: yes; color: no | [shaders/README.md](shaders/README.md) |
| `textures/` | `Minecraft-like_3d_tileset.png` (512×512 RGBA atlas) | Yes | [textures/README.md](textures/README.md) |
| `fonts/` | `PixelOperator8.ttf`, `PixelOperator8-Bold.ttf` | Yes (HUD) | [fonts/README.md](fonts/README.md) |
| `sounds/` | 6 × WAV (8‑bit mono 44.1 kHz) | No | [sounds/README.md](sounds/README.md) |
| `music/` | `time_for_adventure.mp3` | No | [music/README.md](music/README.md) |
| `levels/` | `level1..4.txt` ASCII 2D tile maps | No (no loader exists) | [levels/README.md](levels/README.md) |

The sounds, music and levels (and the engine's `Sprite`/`Animation` classes) are leftovers from
an earlier 2D platformer phase; the commit that added them is titled "version1.0 with 2d and 3d
assets".
