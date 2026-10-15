# assets/music/

Background music for `mge::AudioManager::playMusic` (streamed by SDL_mixer).
**Not used by the current voxel game.**

| File | Format |
|------|--------|
| `time_for_adventure.mp3` | MP3, 320 kbps, 44.1 kHz, joint stereo (~1.6 MB) |

```cpp
mge::AudioManager::playMusic("../assets/music/time_for_adventure.mp3");   // loops forever (-1)
```
MP3 playback requires SDL2_mixer to be built with an MP3 decoder (the Fedora package is).
