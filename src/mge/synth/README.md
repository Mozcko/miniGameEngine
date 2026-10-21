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
| `Sfx.h/.cpp` | `SfxParams`, `renderSfx`, `sfx::` presets (jump, coin, hurt, explosion, powerUp, click), `randomize`, `mutate` |
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

## Sound effects, sfxr‑style (#48)
An effect is a **data struct** rendered once into a `Sample`, so playing it is as cheap as a WAV:

```cpp
const mge::SampleId jump = audio->addSfx(mge::sfx::jump());      // = addSample(renderSfx(params, rate))
audio->play(jump);
mge::SfxParams laser = mge::sfx::randomize(42);                  // or build one field by field
```

**`SfxParams`:**
- **Wave and duty:** waveform, duty and duty sweep.
- **Pitch:** base frequency, slide and delta slide (octaves/s and octaves/s²), a minimum frequency
  that ends the sound, vibrato, an arpeggio (multiplier after a time), and a repeat that restarts
  pitch and arpeggio.
- **Envelope:** attack, sustain with punch, decay.
- **Filters and phaser:** low‑pass (cutoff, sweep, resonance), high‑pass (cutoff, sweep), and a
  phaser (offset, sweep in ms).
- **Gain.**

**`renderSfx(params, rate, seed)`:**
- returns mono, `duration()` = attack + sustain + decay;
- deterministic per seed (noise);
- runs on the game thread, because it allocates.

**Presets** (code equivalents of `assets/sounds/*.wav`):

| Preset | Sound |
|--------|-------|
| `jump` | Square that rises ~7 octaves/s |
| `coin` | B5 then E6 (arpeggio), with punch |
| `hurt` | Saw falling fast, low‑passed |
| `explosion` | Low pitched noise, falling, a low‑pass sweeping down and a phaser |
| `powerUp` | Rising square that restarts every 0.11 s, with vibrato |
| `click` | A 40 ms blip, high‑passed |

**Variation:** `sfx::coin(seed)` with a seed other than 0 is a slight variation (`mutate(params, 0.1,
seed)`). `mutate(params, amount, seed)` and `randomize(seed)` make new effects with sensible ranges.

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
- echo + vibrato + a falling pitch slide;
- `sfx_*`: every preset and three random effects (#48).

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

`tests/synth/test_sfx.cpp` (#48):
- every preset: its duration, determinism, range, and a quiet ending;
- jump rises, hurt falls, coin is B5 then E6;
- the explosion is noise whose centroid falls;
- power‑up rises, restarts and rises again;
- click is under 60 ms;
- seeded variations are slight and deterministic; 40 random effects are audible and finite.

`tests/synth/test_no_alloc.cpp` (its own program, `mge_synth_alloc_tests`; not built with sanitizers,
which replace `operator new` themselves) replaces global `operator new` and checks that
`Mixer::render` allocates nothing with notes, samples, tones, the echo and voice stealing.

`tests/core/test_spsc_queue.cpp` covers the queue, including a 200 000‑item two‑thread stress test.
