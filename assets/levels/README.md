# assets/levels/

ASCII tile maps for a 2D side‑scrolling platformer. **No code in the repository loads or parses
these files** – they are kept from an earlier 2D phase of the project. The legend below is
**inferred from the file contents**, not from a parser, so treat it as a best guess.

| File | Header | Width × rows |
|------|--------|--------------|
| `level1.txt` | `#BIOME GRASS` | 68 × 10 |
| `level2.txt` | `#BIOME SAND` | 74 × 10 |
| `level3.txt` | `#BIOME RED_SAND` | 74 × 10 |
| `level4.txt` | `#BIOME ICE` | 80 × 10 |

## Format
- Line 1: `#BIOME <NAME>` – selects the visual theme for the level.
- Following lines: one character per tile, top row first; the last two rows are the ground.

## Probable legend

| Char | Likely meaning |
|------|----------------|
| `.` | Empty / air |
| `@` | Player spawn |
| `?` | Item / question block (coin or power‑up) |
| `!` | Special pickup or checkpoint |
| `E` | Enemy spawn |
| `G` | Ground surface tile (biome‑themed top) |
| `D` | Dirt / underground tile |
| `[` `-` `]` | Left end / middle / right end of a bridge or platform over a gap |
| `1`, `2` | Numbered platforms or pillar markers in the pit section |
| `~` | Water / liquid (level 2) |
| `I` | Ice tile (level 4) |
| `S` / `s` | Snow surface / snow underground (level 4) |

If you implement a loader, document the definitive legend here.
