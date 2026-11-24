#pragma once

#include <mge/synth/Oscillator.h>
#include <mge/synth/Sample.h>

#include <cstdint>

namespace mge {

    // Parameters for an sfxr-style sound effect (#48). An effect is plain data: defined in code,
    // rendered once with renderSfx, and played like any sample (as cheap as a WAV).
    struct SfxParams {
        Waveform wave = Waveform::Square;
        float duty = 0.5f;      // Pulse width (square wave)
        float dutySweep = 0.0f; // Duty change per second

        float baseFrequency = 440.0f;    // Starting Hz
        float minFrequency = 20.0f;      // If slide drops below this, the sound ends
        float slide = 0.0f;              // Octaves per second (+ up, − down)
        float deltaSlide = 0.0f;         // Slide acceleration, octaves/s²
        float vibratoDepth = 0.0f;       // Semitones
        float vibratoSpeed = 0.0f;       // Hz
        float arpeggioMultiplier = 1.0f; // Frequency is multiplied by this after arpeggioTime (1 = off)
        float arpeggioTime = 0.0f;       // Seconds
        float repeatTime = 0.0f;         // Restarts pitch and arpeggio at this interval (0 = off)

        // Envelope: rises in attack, holds sustain (with extra initial punch), falls in decay
        float attack = 0.0f;
        float sustain = 0.1f;
        float punch = 0.0f; // 0..1
        float decay = 0.2f;

        float lowPassCutoff = 0.0f; // Hz (0 = no filter)
        float lowPassSweep = 0.0f;  // Octaves per second
        float lowPassResonance = 0.0f;
        float highPassCutoff = 0.0f; // Hz (0 = no filter)
        float highPassSweep = 0.0f;  // Octaves per second
        float phaserOffset = 0.0f;   // Milliseconds (0 = no phaser)
        float phaserSweep = 0.0f;    // Milliseconds per second

        float gain = 0.5f;

        float duration() const { return attack + sustain + decay; }
    };

    // Renders the effect (mono) at sampleRate. Deterministic: same input and seed → same samples.
    // Runs on the game thread (allocates): register the result with addSample/AudioModule::addSfx
    Sample renderSfx(const SfxParams& params, int sampleRate = kDefaultSampleRate, std::uint64_t seed = 0);

    // Ready-made presets, equivalents of the WAVs in assets/sounds. With seed != 0 produces slight
    // variations (mutate), useful to avoid repetition
    namespace sfx {
        SfxParams jump(std::uint64_t seed = 0);
        SfxParams coin(std::uint64_t seed = 0);
        SfxParams hurt(std::uint64_t seed = 0);
        SfxParams explosion(std::uint64_t seed = 0);
        SfxParams powerUp(std::uint64_t seed = 0);
        SfxParams click(std::uint64_t seed = 0);

        // A random effect (with sensible ranges) and a variation of another: amount 0..1
        SfxParams randomize(std::uint64_t seed);
        SfxParams mutate(const SfxParams& params, float amount, std::uint64_t seed);
    }
}
