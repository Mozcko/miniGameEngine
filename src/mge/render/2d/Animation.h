#pragma once
#include <vector>

namespace mge {

    class Animation {
    public:
        // speed: Cuantos SEGUNDOS dura cada frame (ej: 0.1f)
        // loop: Si es true, la animación vuelve al inicio al terminar
        Animation(float speed = 0.1f, bool loop = true);

        void addFrame(int tileIndex);
        void update(float deltaTime);
        int getCurrentTile() const;

    private:
        std::vector<int> m_frames;
        int m_currentFrameIndex;
        float m_speed;
        float m_timeElapsed;
        bool m_loop;
    };
}