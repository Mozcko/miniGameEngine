# assets/music/

Background music. **Not playable at the moment**: SDL_mixer was removed in #9 and
`mge::AudioManager::playMusic` only logs a warning until the procedural music sequencer (#49).
**Not used by the current voxel game.**

| File | Format |
|------|--------|
| `time_for_adventure.mp3` | MP3, 320 kbps, 44.1 kHz, joint stereo (~1.6 MB) |

```cpp
mge::AudioManager::playMusic("../assets/music/time_for_adventure.mp3");   // loops forever (-1)
```
