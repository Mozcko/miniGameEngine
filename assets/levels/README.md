# assets/levels/

ASCII tile maps for a 2D side‑scrolling platformer, kept from the project's earlier 2D phase as
**test data for the 2D path**. `examples/platformer2d` loads them (`platformer::loadLevel`, #45),
and `tests/examples/test_platformer.cpp` checks that all four parse.

| File | Header | Width × rows |
|------|--------|--------------|
| `level1.txt` | `#BIOME GRASS` | 68 × 10 |
| `level2.txt` | `#BIOME SAND` | 74 × 10 |
| `level3.txt` | `#BIOME RED_SAND` | 74 × 10 |
| `level4.txt` | `#BIOME ICE` | 80 × 10 |

## Format
- Line 1: `#BIOME <NAME>` – selects the visual theme for the level.
- Following lines: one character per tile, top row first; the last two rows are the ground.

## Legend (as implemented by `examples/platformer2d/Level.cpp`)

| Char | Meaning in the loader |
|------|----------------------|
| `.` | Empty |
| `@` | Player spawn (required) |
| `?` | Coin (collected on touch, `coin.wav`) |
| `!` | Special pickup (counts as a coin) |
| `E` | Enemy: drops to the ground below it; touching it sends you back to the spawn |
| `G` / `D` | Ground surface / dirt below it (solid) |
| `[` `-` `]` | Bridge over a gap: left end, middle, right end (solid) |
| `1`, `2` | Pillars in the pit section (solid) |
| `~` | Water (level 2): not solid; falling in sends you back to the spawn |
| `I` | Ice (level 4, solid) |
| `S` / `s` | Snow surface / snow underground (level 4, solid) |

Any other character is a parse error. Shorter rows are padded with empty cells, and Windows line
endings are accepted. Reaching the right edge loads the next level.
