#pragma once

#include <cstdint>

namespace mge {

    enum class FilterMode : std::uint8_t {
        None,
        LowPass,
        HighPass,
        BandPass
    };

    // Filtro de estado variable (SVF "TPT", #47): pasa bajos, pasa altos o pasa banda de 12 dB/octava,
    // estable con cualquier corte y resonancia. Sin memoria dinámica (hilo de audio)
    class Filter {
    public:
        // cutoff en Hz (se limita a [10, 0.45 × sampleRate]); resonance 0..1 (0 = sin pico)
        void set(FilterMode mode, float cutoff, float resonance, int sampleRate);
        void reset() { m_ic1 = m_ic2 = 0.0f; }
        float process(float input);
        FilterMode mode() const { return m_mode; }

    private:
        FilterMode m_mode = FilterMode::None;
        float m_g = 0.0f, m_k = 2.0f, m_a1 = 0.0f, m_a2 = 0.0f, m_a3 = 0.0f;
        float m_ic1 = 0.0f, m_ic2 = 0.0f;
    };
}
