#include <mge/render/2d/Animation.h>

namespace mge {

    Animation::Animation(float speed, bool loop) 
        : m_currentFrameIndex(0), m_speed(speed), m_timeElapsed(0.0f), m_loop(loop)
    {}

    void Animation::addFrame(int tileIndex) {
        m_frames.push_back(tileIndex);
    }

    void Animation::update(float deltaTime) {
        // Sin frames no hay nada que animar (evita underflow de size() - 1)
        if (m_frames.empty()) return;

        m_timeElapsed += deltaTime;
        const int frameCount = static_cast<int>(m_frames.size());

        // Si pasó el tiempo suficiente, avanzamos al siguiente frame
        if (m_timeElapsed >= m_speed) {
            m_timeElapsed -= m_speed; // Restamos en lugar de poner 0 para no perder tiempo sobrante

            // Solo avanzamos si estamos en loop O si no hemos llegado al final
            if (m_loop || m_currentFrameIndex < frameCount - 1) {
                m_currentFrameIndex++;
            }

            // Si llegamos al final, volvemos al principio (Loop)
            if (m_currentFrameIndex >= frameCount) {
                if (m_loop) m_currentFrameIndex = 0;
                else m_currentFrameIndex = frameCount - 1;
            }
        }
    }

    int Animation::getCurrentTile() const {
        if (m_frames.empty()) return 0;
        return m_frames[m_currentFrameIndex];
    }
}