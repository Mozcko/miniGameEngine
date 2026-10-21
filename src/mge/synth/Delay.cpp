#include <mge/synth/Delay.h>

#include <algorithm>

namespace mge {

    Delay::Delay(float maxSeconds, int sampleRate)
        : m_frames(std::max<std::size_t>(2, std::size_t(std::max(maxSeconds, 0.01f) * float(sampleRate)))),
          m_sampleRate(sampleRate) {
        m_buffer.assign(m_frames * 2, 0.0f);
        setSettings(m_settings);
    }

    void Delay::setSettings(const DelaySettings& settings) {
        m_settings = settings;
        m_settings.feedback = std::clamp(settings.feedback, 0.0f, 0.95f);
        m_settings.mix = std::clamp(settings.mix, 0.0f, 1.0f);
        m_delayFrames =
            std::clamp<std::size_t>(std::size_t(settings.time * float(m_sampleRate)), 1, m_frames - 1);
    }

    void Delay::clear() {
        std::fill(m_buffer.begin(), m_buffer.end(), 0.0f);
    }

    void Delay::process(float inLeft, float inRight, float& outLeft, float& outRight) {
        const std::size_t read = (m_write + m_frames - m_delayFrames) % m_frames;
        const float echoLeft = m_buffer[read * 2];
        const float echoRight = m_buffer[read * 2 + 1];
        m_buffer[m_write * 2] = inLeft + echoLeft * m_settings.feedback;
        m_buffer[m_write * 2 + 1] = inRight + echoRight * m_settings.feedback;
        m_write = (m_write + 1) % m_frames;
        outLeft = echoLeft * m_settings.mix;
        outRight = echoRight * m_settings.mix;
    }
}
