#include <mge/core/Time.h>

namespace mge {
    // Reloj monotónico de la librería estándar: el core no depende de SDL
    std::chrono::steady_clock::time_point Time::s_lastTime = std::chrono::steady_clock::now();
    float Time::s_deltaTime = 0.0f;
    float Time::s_rawDeltaTime = 0.0f;
    float Time::s_timeScale = 1.0f;

    void Time::init() {
        s_lastTime = std::chrono::steady_clock::now();
    }

    void Time::update() {
        auto currentTime = std::chrono::steady_clock::now();
        std::chrono::duration<float> elapsed = currentTime - s_lastTime;
        s_lastTime = currentTime;
        advance(elapsed.count());
    }

    void Time::advance(float rawDeltaSeconds) {
        s_rawDeltaTime = rawDeltaSeconds;

        // Protección contra saltos grandes de tiempo (ej: mover ventana)
        if (s_rawDeltaTime > kMaxDeltaTime) s_rawDeltaTime = kMaxDeltaTime;
        if (s_rawDeltaTime < 0.0f) s_rawDeltaTime = 0.0f;

        s_deltaTime = s_rawDeltaTime * s_timeScale;
    }

    float Time::getDeltaTime() {
        return s_deltaTime;
    }
    float Time::getUnscaledDeltaTime() {
        return s_rawDeltaTime;
    }
    float Time::getTimeScale() {
        return s_timeScale;
    }
    void Time::setTimeScale(float scale) {
        s_timeScale = scale;
    }
}
