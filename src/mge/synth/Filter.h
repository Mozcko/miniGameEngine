#pragma once

#include <cstdint>

namespace mge {

    enum class FilterMode : std::uint8_t {
        None,
        LowPass,
        HighPass,
        BandPass
    };

    // State-variable filter (TPT SVF, #47): 12 dB/octave low-pass, high-pass, or band-pass,
    // stable at any cutoff and resonance. No dynamic allocation (audio thread)
    class Filter {
    public:
        // cutoff in Hz (clamped to [10, 0.45 × sampleRate]); resonance 0..1 (0 = no peak)
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
