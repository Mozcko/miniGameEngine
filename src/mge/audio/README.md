# src/mge/audio/  →  target `mge_audio`

| File | Namespace | Purpose |
|------|-----------|---------|
| `AudioManager.h/.cpp` | `mge` | Static audio on **SDL3 core audio** (no SDL_mixer): fire‑and‑forget WAV sound effects through `SDL_AudioStream`, cached by file path |

Depends on: `mge_core` (logging), SDL3. This is a stop‑gap: M5 replaces it with procedural
synthesis (#46 SDL3 audio streams + synth, #48 SFX generator, #49 music sequencer), following the
zero‑asset policy ([ADR 0002](../../../docs/adr/0002-zero-asset-policy.md)).

## `mge::AudioManager` (all static)

| Method | Behaviour |
|--------|-----------|
| `init()` | `SDL_InitSubSystem(SDL_INIT_AUDIO)` and opens the default playback device. Called by the `Application` constructor. Failure is logged and the game continues **silently** (every call becomes a no‑op). |
| `shutdown()` | Destroys active streams, frees cached WAV data (`SDL_free`), closes the device, quits the audio subsystem. Called by `~Application`. |
| `playSound(path, volume = 128)` | Loads the WAV with `SDL_LoadWAV` on first use (cached), creates an `SDL_AudioStream` (WAV format → device format), binds it to the device, sets gain `volume/128` (0–128, same scale as before), queues the data. One stream per playing sound; finished streams are released lazily on the next call. |
| `playMusic(path, loops)` | **Not supported** until #49 – logs a warning and does nothing. |
| `pauseMusic()` / `resumeMusic()` / `stopMusic()` | No‑ops (see above). |

### Notes
- Only WAV is supported (`SDL_LoadWAV`); MP3/OGG decoding went away with SDL_mixer.
- Paths are relative to the working directory, e.g. `"../assets/sounds/jump.wav"` from `build/`.
- No game code plays anything yet. Available files: `assets/sounds/*.wav`.
- Headless/CI: run with `SDL_AUDIODRIVER=dummy`.

### Example
```cpp
mge::AudioManager::playSound("../assets/sounds/jump.wav", 96);
```
