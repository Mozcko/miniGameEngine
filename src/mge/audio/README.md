# src/mge/audio/

| File | Namespace | Purpose |
|------|-----------|---------|
| `AudioManager.h/.cpp` | `MiniGameEngine` | Static SDL_mixer wrapper: fire‑and‑forget sound effects and streamed background music, each cached by file path |

## `MiniGameEngine::AudioManager` (all static)

| Method | Behaviour |
|--------|-----------|
| `init()` | `Mix_OpenAudio(44100 Hz, MIX_DEFAULT_FORMAT, 2 ch, 2048 chunk)`. Called by the `Application` constructor. Failure is logged, not fatal. |
| `shutdown()` | Frees every cached `Mix_Chunk` / `Mix_Music`, `Mix_CloseAudio()`. Called by `~Application`. |
| `playSound(path, volume = 128)` | Loads (`Mix_LoadWAV`) on first use, caches, sets chunk volume (0–128), plays on the first free channel. Logs if no channel is free. |
| `playMusic(path, loops = -1)` | Loads (`Mix_LoadMUS`, MP3/OGG/…) on first use, caches, plays. `-1` = loop forever, `0` = once. |
| `pauseMusic()` / `resumeMusic()` / `stopMusic()` | Thin wrappers over `Mix_PauseMusic`, `Mix_ResumeMusic`, `Mix_HaltMusic`. |

### Notes
- Volume is set **on the chunk**, so playing the same SFX at different volumes affects all
  instances currently playing.
- Paths are passed straight to SDL_mixer → relative to the working directory, e.g.
  `"../assets/sounds/jump.wav"` when run from `build/`.
- No game code currently plays anything. Available files: `assets/sounds/*.wav`,
  `assets/music/time_for_adventure.mp3` (see `assets/README.md`).

### Example
```cpp
MiniGameEngine::AudioManager::playMusic("../assets/music/time_for_adventure.mp3");
MiniGameEngine::AudioManager::playSound("../assets/sounds/jump.wav", 96);
```
