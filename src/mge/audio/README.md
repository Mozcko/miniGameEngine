# src/mge/audio/  →  target `mge_audio`

| File | Namespace | Purpose |
|------|-----------|---------|
| `AudioModule.h/.cpp` | `mge` | Plays the engine's `Mixer` (from [`mge_synth`](../synth/README.md)) on the default playback device through an SDL3 audio stream, and loads WAV files into it |

Depends on `mge_core`, `mge_synth` and SDL3. All the mixing and synthesis lives in the headless
`mge_synth`; this module is only the device glue. Sound can come from code (tones, and in later M5
issues synth notes, SFX and music) **or** from WAV files: both go through the same mixer, volume
and mute ([ADR 0004](../../../docs/adr/0004-procedural-first-assets-optional.md)).

## `mge::AudioModule`
`addModule<mge::AudioModule>()`; then `auto* audio = getModule<mge::AudioModule>()`.

| Member | Behaviour |
|--------|-----------|
| `onInit` | `SDL_InitSubSystem(SDL_INIT_AUDIO)`, then `SDL_OpenAudioDeviceStream(default playback, float32 stereo 48 kHz, callback)` and resume it. **Never fails:** without a device the game runs silently (`hasDevice()` = false). `MGE_MUTE=1` starts muted |
| `onShutdown` | Destroys the stream (closes the device; the callback stops), quits the audio subsystem (and `SDL_Quit()` if it was the last one) |
| `playTone(ToneDesc{hz, seconds, gain, pan})` | A sine tone with no files |
| `loadWav(path)` → `SampleId` | `SDL_LoadWAV` + `SDL_ConvertAudioSamples` to float32 at the mixer rate (mono stays mono). Relative paths go through `mge::assetPath`. `kInvalidSample` on failure (logged) |
| `addSample(Sample)` → `SampleId` | Register a sound made by code |
| `play(id, gain = 1, pan = 0)` | Plays a sample; pan −1..1 at constant power |
| `stopAll()` | Silences every voice |
| `setMasterVolume(0..1)`, `setMuted(bool)` | Smoothed per block (no clicks) |
| `stats()` | Active voices, dropped commands, stolen voices |
| `mixer()` | The `Mixer` itself |

### Threads
SDL calls `feed` on its audio thread whenever the device needs data. It renders the mixer in blocks
of 1 024 frames into a buffer reserved in `onInit` and pushes them with `SDL_PutAudioStreamData`.
The game thread talks to the mixer only through its lock‑free command queue (see `mge_synth`), so
a sound starts at most one device block later (a few ms). Call the module from one game thread.

### Headless / CI
`SDL_AUDIODRIVER=dummy` opens a real (silent) device; tests and smoke tests use it.
`tests/audio/test_audio_module.cpp` opens it, converts `assets/sounds/jump.wav`, plays and closes.

### Example
```cpp
auto* audio = getModule<mge::AudioModule>();
audio->playTone({440.0f, 0.3f});                     // no files
const mge::SampleId jump = audio->loadWav("sounds/jump.wav");
audio->play(jump, 0.5f, -0.3f);                       // half volume, a bit to the left
```
