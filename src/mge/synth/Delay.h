#pragma once

#include <cstddef>
#include <vector>

namespace mge {

    struct DelaySettings {
        float time = 0.3f;      // Seconds (up to the Delay's maximum)
        float feedback = 0.35f; // 0..0.95
        float mix = 0.0f;       // How much of the delay line is added to output (0 = off)
    };

    // Stereo echo (#47) with a fixed maximum delay line: memory is preallocated on construction (game
    // thread); process() does not allocate. The mixer owns one as a send effect
    class Delay {
    public:
        Delay(float maxSeconds, int sampleRate);

        void setSettings(const DelaySettings& settings);
        const DelaySettings& settings() const { return m_settings; }
        // Input (inLeft, inRight); returns the wet echo signal to add (already scaled by mix)
        void process(float inLeft, float inRight, float& outLeft, float& outRight);
        void clear();

    private:
        std::vector<float> m_buffer; // Interleaved stereo
        std::size_t m_frames = 0;
        std::size_t m_write = 0;
        std::size_t m_delayFrames = 1;
        int m_sampleRate;
        DelaySettings m_settings;
    };
}
