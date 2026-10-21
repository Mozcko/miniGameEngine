# src/mge/synth/  →  target `mge_synth`

**Procedural audio on the CPU, headless** (`mge_core` only). It is always built, including in the
core‑only and ThreadSanitizer CI jobs, so everything here is tested without a sound device.
[`mge_audio`](../audio/README.md) plays it on the device. Programs that link it see
`MGE_HAS_SYNTH`, and `<mge/mge.h>` then includes it.

| File | Contents |
|------|----------|
| `Sample.h` | `Sample` (float frames, mono or stereo interleaved), `SampleId`, `kAudioChannels` = 2, `kDefaultSampleRate` = 48 000 |
| `Mixer.h/.cpp` | `Mixer`, `ToneDesc`, `MixerStats`, `panGains` |

## Format
Float32 in [−1, 1], stereo interleaved (L, R, L, R…), 48 kHz by default (a `Mixer` constructor
argument). Samples must already be at the mixer's rate (`AudioModule::loadWav` converts).

## `Mixer` (#46)
- **Game thread:**
  - `playTone`, `playSample(id, gain, pan)` and `stopAll` push POD commands into a lock‑free
    `SpscQueue` (`<mge/core/SpscQueue.h>`, 255 entries). They return false when the queue is full;
    the command is dropped and counted in `stats().droppedCommands`.
  - `addSample(Sample)` → `SampleId` registers a sound in a fixed bank of 256.
  - `setMasterVolume(0..1)` and `setMuted` are atomics.
- **Audio thread:** `render(out, frames)` applies the queued commands, mixes the voices, applies the
  master gain (a linear ramp to the new value over the block, so changes don't click) and clips to
  [−1, 1].
- **Rules for `render`:**
  - **no allocations, locks or logging**;
  - the 32 voices and the sample bank are preallocated, and when every voice is busy the oldest is
    stolen (`stats().stolenVoices`);
  - samples are immutable once registered and live until the mixer is destroyed, so the audio
    thread never frees memory.
- **Voices:**
  - a sine tone with a 5 ms fade in and out;
  - a sample player (a mono sample plays on both sides; pan is constant‑power, with gain 1 at the
    centre).
- **One game thread** may call the game‑side API (single producer).

## Tests
`tests/synth/test_mixer.cpp`:
- tone frequency (zero crossings), amplitude, duration and the fade‑in;
- pan, master volume ramp, mute;
- a sample plays once, exactly, across several blocks;
- `stopAll`; queue overflow and voice stealing;
- a game thread enqueuing while another thread renders (also under TSan).

`tests/core/test_spsc_queue.cpp` covers the queue, including a 200 000‑item two‑thread stress test.
