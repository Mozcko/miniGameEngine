#include <mge/synth/Filter.h>

#include <algorithm>
#include <cmath>

namespace mge {

    void Filter::set(FilterMode mode, float cutoff, float resonance, int sampleRate) {
        m_mode = mode;
        const float rate = float(sampleRate);
        const float fc = std::clamp(cutoff, 10.0f, 0.45f * rate);
        m_g = std::tan(3.14159265f * fc / rate);
        // Q de 0.5 (sin pico) a ~20; k = 1/Q
        const float q = 0.5f + std::clamp(resonance, 0.0f, 1.0f) * 19.5f;
        m_k = 1.0f / q;
        m_a1 = 1.0f / (1.0f + m_g * (m_g + m_k));
        m_a2 = m_g * m_a1;
        m_a3 = m_g * m_a2;
    }

    float Filter::process(float input) {
        if (m_mode == FilterMode::None) return input;
        const float v3 = input - m_ic2;
        const float v1 = m_a1 * m_ic1 + m_a2 * v3;
        const float v2 = m_ic2 + m_a2 * m_ic1 + m_a3 * v3;
        m_ic1 = 2.0f * v1 - m_ic1;
        m_ic2 = 2.0f * v2 - m_ic2;
        switch (m_mode) {
        case FilterMode::LowPass: return v2;
        case FilterMode::BandPass: return v1;
        case FilterMode::HighPass: return input - m_k * v1 - v2;
        case FilterMode::None: break;
        }
        return input;
    }
}
