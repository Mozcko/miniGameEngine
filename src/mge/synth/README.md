# src/mge/synth/  →  target `mge_synth`

**Procedural audio on the CPU, headless** (`mge_core` only). It is always built, including in the
core‑only and ThreadSanitizer CI jobs, so everything here is tested without a sound device.
[`mge_audio`](../audio/README.md) plays it on the device. Programs that link it see
`MGE_HAS_SYNTH`, and `<mge/mge.h>` then includes it.

| File | Contents |
|------|----------|
| `Sample.h` | `Sample` (float frames, mono or stereo interleaved), `SampleId`, `kAudioChannels` = 2, `kDefaultSampleRate` = 48 000 |
| `Oscillator.h/.cpp` | `Waveform` (sine, square with pulse width, saw, triangle, noise, 8‑bit loop noise), `Oscillator`, `midiToHz` |
| `Envelope.h/.cpp` | `Adsr`, `Envelope` (linear ADSR) |
| `Filter.h/.cpp` | `FilterMode`, `Filter` (state‑variable low/high/band‑pass with resonance) |
| `Delay.h/.cpp` | `DelaySettings`, `Delay` (stereo echo, buffer reserved up front) |
| `Instrument.h` | `Instrument`: how a note sounds (wave, ADSR, filter, gain, pan, vibrato, pitch slide, delay send) |
| `Mixer.h/.cpp` | `Mixer`, `ToneDesc`, `AudioBus`, `VoiceHandle`, `MixerStats`, `panGains`, `softClip` |
| `Offline.h/.cpp` | `renderOffline`, `writeWav` (16‑bit PCM, no deps), `analysis::` (rms, peak, frequency, tone amplitude, spectral centroid, onsets) |

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

## Synthesizer (#47)
Every building block runs per sample with **no allocations**, so it can live on the audio thread.

| Block | Notes |
|-------|-------|
| `Oscillator` | Phase accumulator (`setFrequency(hz, rate)`, `next()` in [−1, 1]). Square and saw use **PolyBLEP** (less aliasing). Noise is "pitched" like sfxr: a new value 32 times per cycle; `NoiseLoop` is the NES short‑mode LFSR (periodic, metallic). `reset(seed)` makes noise deterministic |
| `Envelope` | Linear attack → decay → sustain until `noteOff()` → release. `sustain = 0` makes it percussive (it finishes by itself) |
| `Filter` | TPT state‑variable filter, 12 dB/oct; cutoff clamped to [10 Hz, 0.45 × rate], resonance 0..1 (Q 0.5–20). Stable at any setting |
| `Delay` | Stereo echo; `time`, `feedback` (≤ 0.95) and `mix`. The mixer has one, fed by each note's `delaySend`; `mixer.setDelay(settings)` |

**Notes:**
- `playNote(instrument, hz, velocity, duration, bus)` returns a `VoiceHandle`.
- `duration > 0`: the note releases by itself after that many seconds, then plays its release.
- `duration <= 0`: the note holds until `releaseNote(handle)`. `stopNote(handle)` cuts it at once.
- MIDI notes: `midiToHz(60)` = middle C.

**Voice stealing:** with all 32 voices busy, a new sound takes the quietest voice that is already
releasing, otherwise the oldest.

**Buses:**
- `AudioBus::Effects` (tones, notes and samples by default) and `AudioBus::Music`, each with
  `setBusVolume`.
- After the master gain, `softClip` keeps the output linear up to 0.8 and bends smoothly towards
  ±1 instead of hard clipping.

```cpp
mge::Instrument lead;
lead.wave = mge::Waveform::Square;
lead.envelope = {0.005f, 0.08f, 0.6f, 0.15f};
lead.filter = mge::FilterMode::LowPass;
lead.cutoff = 2500.0f;
lead.vibratoDepth = 0.15f;                 // semitones
audio->playNote(lead, mge::midiToHz(72), 0.8f, 0.3f);
```

## Offline rendering and analysis (#50)
Audio can be produced and checked without a device:
- `renderOffline(mixer, frames | seconds)` runs `render` on the calling thread and returns the
  interleaved stereo buffer.
- `writeWav(path, samples, channels, rate)` writes 16‑bit PCM, with no dependencies (like
  `writePng`).

**`mge::analysis::`** measures mono signals (`channel(buffer, 2, 0)` extracts one):

| Function | Measures |
|----------|----------|
| `rms`, `peak` | Level |
| `zeroCrossingHz` | Pitch of a signal with one dominant tone |
| `toneAmplitude(hz)` | Amplitude of one frequency (Goertzel) |
| `spectralCentroid` | "Brightness": Hann‑windowed DFT over 2 048 samples |
| `onsets(threshold)` | Note starts (5 ms RMS windows crossing the threshold) |
| `lastAbove` | Where the sound ends |

**Golden values, not golden files:**
- Tests compare these *features*, with tolerances, so they hold across GCC/Clang and optimisation
  levels.
- Exact output is only checked for determinism: the same commands give byte‑identical buffers.

**`examples/audio_gallery`** (headless, always built, smoke test) writes
`<build>/gallery/audio/*.wav` plus a spectrogram `.png` of each. The spectrogram shows time on x,
0–8 kHz on y, and −60..0 dB as brightness. It covers:
- every waveform (an arpeggio);
- three ADSR shapes;
- a resonant filter sweep;
- echo + vibrato + a falling pitch slide.

## Tests
`tests/synth/test_mixer.cpp`:
- tone frequency (zero crossings), amplitude, duration and the fade‑in;
- pan, master volume ramp, mute;
- a sample plays once, exactly, across several blocks;
- `stopAll`; queue overflow and voice stealing;
- a game thread enqueuing while another thread renders (also under TSan).

`tests/synth/test_synth.cpp` (#47):
- every waveform's frequency, range and known RMS; pulse width; deterministic, centred noise; the
  8‑bit loop's period;
- ADSR timings; filter responses and stability at full resonance; echo timing and feedback;
- notes ending after duration + release; held notes, release and stop; bus volume;
- stealing a releasing voice first; the echo send; pitch slide; `softClip`.

`tests/synth/test_offline.cpp` (#50):
- `renderOffline` size and determinism;
- `writeWav` header and sample values, including clipping;
- per waveform: frequency, tone amplitude, centroid order sine < triangle < square < saw < noise;
  the sine's exact level; the square's 1/3 third harmonic and no even harmonics;
- the envelope over time and the note's end; a lower low‑pass cutoff gives a darker sound;
- onsets of four notes.

`tests/synth/test_no_alloc.cpp` (its own program, `mge_synth_alloc_tests`; not built with sanitizers,
which replace `operator new` themselves) replaces global `operator new` and checks that
`Mixer::render` allocates nothing with notes, samples, tones, the echo and voice stealing.

`tests/core/test_spsc_queue.cpp` covers the queue, including a 200 000‑item two‑thread stress test.
