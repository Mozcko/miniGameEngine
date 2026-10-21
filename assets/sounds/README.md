# assets/sounds/

Short sound effects for `mge::AudioManager::playSound`. All are RIFF WAV, PCM 8‑bit,
mono, 44.1 kHz. They come from the earlier 2D platformer phase. The voxel sandbox doesn't use
them; `examples/platformer2d` plays `jump`, `coin`, `hurt` and `power_up` (#45).

| File | Intended use (by name) |
|------|------------------------|
| `coin.wav` | Collecting a coin/pickup |
| `explosion.wav` | Explosion (e.g. TNT) |
| `hurt.wav` | Player takes damage |
| `jump.wav` | Jump |
| `power_up.wav` | Power‑up |
| `tap.wav` | UI click / footstep |

```cpp
mge::AudioManager::playSound(mge::assetPath("sounds/jump.wav").string());  // volume 0..128, default 128
```
