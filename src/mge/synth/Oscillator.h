#pragma once

#include <mge/core/Random.h>

#include <cstdint>

namespace mge {

    enum class Waveform : std::uint8_t {
        Sine,
        Square, // Pulse with variable duty cycle (pulseWidth)
        Saw,    // Rising sawtooth
        Triangle,
        Noise,     // White noise
        NoiseLoop, // Periodic 8-bit noise (short LFSR, metallic like retro consoles)
    };

    // Phase-accumulator oscillator (#47). Square and saw use PolyBLEP for less high-frequency aliasing.
    // No dynamic allocation: safe to run on the audio thread.
    class Oscillator {
    public:
        void setWaveform(Waveform wave) { m_wave = wave; }
        Waveform waveform() const { return m_wave; }
        void setFrequency(float hz, int sampleRate) { m_step = double(hz) / double(sampleRate); }
        float frequencyStep() const { return float(m_step); } // Cycles per sample
        void setPulseWidth(float width);                      // 0.05..0.95 (0.5 = square)
        void reset(std::uint64_t seed = 0, double phase = 0.0);

        // Next sample in [-1, 1]
        float next();
        double phase() const { return m_phase; }

    private:
        Waveform m_wave = Waveform::Sine;
        double m_phase = 0.0; // [0, 1)
        double m_step = 0.0;  // Cycles per sample
        float m_pulseWidth = 0.5f;
        Pcg32 m_rng{1};
        int m_noiseSlot = -1; // Noise updates 32 times per cycle ("pitched" noise, like sfxr)
        std::uint16_t m_lfsr = 0x7FFF;
        float m_noiseValue = 0.0f;
    };

    // MIDI note (69 = A4 440 Hz) → Hz
    float midiToHz(float note);
}
