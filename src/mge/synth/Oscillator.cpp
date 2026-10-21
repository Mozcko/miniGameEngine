#include <mge/synth/Oscillator.h>

#include <algorithm>
#include <cmath>

namespace mge {

    namespace {
        constexpr double kTwoPi = 6.283185307179586;
        constexpr int kNoiseSlotsPerCycle = 32;

        // Corrección PolyBLEP de un salto en t = 0 (t = fase en ciclos, dt = ciclos por muestra)
        double polyBlep(double t, double dt) {
            if (dt <= 0.0) return 0.0;
            if (t < dt) {
                t /= dt;
                return t + t - t * t - 1.0;
            }
            if (t > 1.0 - dt) {
                t = (t - 1.0) / dt;
                return t * t + t + t + 1.0;
            }
            return 0.0;
        }
    }

    float midiToHz(float note) {
        return 440.0f * std::pow(2.0f, (note - 69.0f) / 12.0f);
    }

    void Oscillator::setPulseWidth(float width) {
        m_pulseWidth = std::clamp(width, 0.05f, 0.95f);
    }

    void Oscillator::reset(std::uint64_t seed, double phase) {
        m_phase = phase - std::floor(phase);
        m_rng = Pcg32(seed + 1);
        m_lfsr = 0x7FFF;
        m_noiseSlot = -1;
        m_noiseValue = 0.0f;
    }

    float Oscillator::next() {
        const double t = m_phase;
        const double dt = std::min(m_step, 0.5);
        double value = 0.0;
        switch (m_wave) {
        case Waveform::Sine: value = std::sin(kTwoPi * t); break;
        case Waveform::Square: {
            const double width = m_pulseWidth;
            value = t < width ? 1.0 : -1.0;
            value += polyBlep(t, dt);
            double fall = t - width;
            if (fall < 0.0) fall += 1.0;
            value -= polyBlep(fall, dt);
            break;
        }
        case Waveform::Saw: value = 2.0 * t - 1.0 - polyBlep(t, dt); break;
        case Waveform::Triangle: value = 1.0 - 4.0 * std::abs(t - 0.5); break;
        case Waveform::Noise:
        case Waveform::NoiseLoop: {
            const int slot = int(t * kNoiseSlotsPerCycle);
            if (slot != m_noiseSlot || m_step * kNoiseSlotsPerCycle >= 1.0) {
                m_noiseSlot = slot;
                if (m_wave == Waveform::Noise) {
                    m_noiseValue = m_rng.nextFloat01() * 2.0f - 1.0f;
                } else {
                    // LFSR de 7 bits (modo "corto" del ruido de la NES): un patrón periódico
                    const std::uint16_t bit = std::uint16_t((m_lfsr ^ (m_lfsr >> 6)) & 1u);
                    m_lfsr = std::uint16_t((m_lfsr >> 1) | (bit << 14));
                    m_noiseValue = (m_lfsr & 1u) ? 1.0f : -1.0f;
                }
            }
            value = m_noiseValue;
            break;
        }
        }
        m_phase += m_step;
        if (m_phase >= 1.0) m_phase -= std::floor(m_phase);
        return float(value);
    }
}
