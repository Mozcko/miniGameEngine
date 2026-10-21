# assets/sounds/

Short sound effects, loaded with `AudioModule::loadWav` (#46). All are RIFF WAV, PCM 8‑bit,
mono, 44.1 kHz. They come from the earlier 2D platformer phase. The voxel sandbox doesn't use
them; `examples/platformer2d` plays `jump`, `coin`, `hurt` and `power_up` (#45) when `S` (or
`PLATFORMER_SOUNDS=wav`) selects the WAVs. Every file has a code‑generated equivalent in `mge::sfx` (#48):
`jump`, `coin`, `hurt`, `explosion`, `powerUp`, `click` (for `tap.wav`).

| File | Intended use (by name) |
|------|------------------------|
| `coin.wav` | Collecting a coin/pickup |
| `explosion.wav` | Explosion (e.g. TNT) |
| `hurt.wav` | Player takes damage |
| `jump.wav` | Jump |
| `power_up.wav` | Power‑up |
| `tap.wav` | UI click / footstep |

```cpp
const mge::SampleId jump = audio->loadWav("sounds/jump.wav");   // once (→ assetPath, float32 48 kHz)
audio->play(jump, 0.5f);                                        // gain 0..1, pan -1..1
```
