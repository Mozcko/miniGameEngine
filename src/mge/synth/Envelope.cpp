#include <mge/synth/Envelope.h>

#include <algorithm>

namespace mge {

    namespace {
        // Muestras de un tramo (al menos 1: un tramo de 0 s salta de una)
        float samplesFor(float seconds, float rate) {
            return std::max(1.0f, seconds * rate);
        }
    }

    void Envelope::start(const Adsr& adsr, int sampleRate) {
        m_adsr = adsr;
        m_adsr.sustain = std::clamp(adsr.sustain, 0.0f, 1.0f);
        m_rate = float(sampleRate);
        m_level = 0.0f;
        m_stage = Stage::Attack;
        m_step = 1.0f / samplesFor(m_adsr.attack, m_rate);
    }

    void Envelope::noteOff() {
        if (m_stage == Stage::Idle || m_stage == Stage::Release) return;
        m_stage = Stage::Release;
        m_step = -std::max(m_level, 1e-6f) / samplesFor(m_adsr.release, m_rate);
    }

    float Envelope::next() {
        const float out = m_level;
        switch (m_stage) {
        case Stage::Idle: return 0.0f;
        case Stage::Attack:
            m_level += m_step;
            if (m_level >= 1.0f) {
                m_level = 1.0f;
                m_stage = Stage::Decay;
                m_step = -(1.0f - m_adsr.sustain) / samplesFor(m_adsr.decay, m_rate);
            }
            break;
        case Stage::Decay:
            m_level += m_step;
            if (m_level <= m_adsr.sustain) {
                m_level = m_adsr.sustain;
                m_stage = Stage::Sustain;
            }
            break;
        case Stage::Sustain:
            if (m_adsr.sustain <= 0.0f) m_stage = Stage::Idle; // Percusivo: termina solo
            break;
        case Stage::Release:
            m_level += m_step;
            if (m_level <= 0.0f) {
                m_level = 0.0f;
                m_stage = Stage::Idle;
            }
            break;
        }
        return out;
    }
}
