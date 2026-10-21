#pragma once

#include <mge/core/Random.h>

#include <cstdint>

namespace mge {

    enum class Waveform : std::uint8_t {
        Sine,
        Square, // Pulso con ancho variable (pulseWidth)
        Saw,    // Sierra subiendo
        Triangle,
        Noise,     // Ruido blanco
        NoiseLoop, // Ruido periódico de 8 bits (LFSR corto, "metálico" como las consolas viejas)
    };

    // Oscilador con acumulador de fase (#47). Cuadrada y sierra con PolyBLEP: menos aliasing en agudos.
    // Sin memoria dinámica: se usa en el hilo de audio.
    class Oscillator {
    public:
        void setWaveform(Waveform wave) { m_wave = wave; }
        Waveform waveform() const { return m_wave; }
        void setFrequency(float hz, int sampleRate) { m_step = double(hz) / double(sampleRate); }
        float frequencyStep() const { return float(m_step); } // Ciclos por muestra
        void setPulseWidth(float width);                      // 0.05..0.95 (0.5 = cuadrada)
        void reset(std::uint64_t seed = 0, double phase = 0.0);

        // Siguiente muestra en [-1, 1]
        float next();
        double phase() const { return m_phase; }

    private:
        Waveform m_wave = Waveform::Sine;
        double m_phase = 0.0; // [0, 1)
        double m_step = 0.0;  // Ciclos por muestra
        float m_pulseWidth = 0.5f;
        Pcg32 m_rng{1};
        int m_noiseSlot = -1; // El ruido cambia 32 veces por ciclo (ruido "afinado", como sfxr)
        std::uint16_t m_lfsr = 0x7FFF;
        float m_noiseValue = 0.0f;
    };

    // Nota MIDI (69 = La 440) → Hz
    float midiToHz(float note);
}
