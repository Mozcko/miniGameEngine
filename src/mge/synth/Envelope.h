#pragma once

#include <cstdint>

namespace mge {

    // ADSR envelope timings, in seconds (sustain: level 0..1)
    struct Adsr {
        float attack = 0.005f;
        float decay = 0.1f;
        float sustain = 0.7f;
        float release = 0.1f;
    };

    // Linear ADSR envelope (#47): rises during attack, falls to sustain during decay, holds until
    // noteOff(), and falls to 0 during release. No dynamic allocation (audio thread)
    class Envelope {
    public:
        enum class Stage : std::uint8_t {
            Idle,
            Attack,
            Decay,
            Sustain,
            Release
        };

        void start(const Adsr& adsr, int sampleRate);
        void noteOff();
        float next();

        float level() const { return m_level; }
        Stage stage() const { return m_stage; }
        bool finished() const { return m_stage == Stage::Idle; }
        bool releasing() const { return m_stage == Stage::Release; }

    private:
        Adsr m_adsr;
        float m_rate = 48000.0f;
        Stage m_stage = Stage::Idle;
        float m_level = 0.0f;
        float m_step = 0.0f; // Change per sample in the current stage
    };
}
