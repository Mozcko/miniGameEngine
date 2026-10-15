#include <mge/core/Time.h>

namespace MiniGameEngine {
    // Reloj monotónico de la librería estándar: el core no depende de SDL
    std::chrono::steady_clock::time_point Time::_lastTime = std::chrono::steady_clock::now();
    float Time::_deltaTime = 0.0f;
    float Time::_rawDeltaTime = 0.0f;
    float Time::_timeScale = 1.0f;

    void Time::init() {
        _lastTime = std::chrono::steady_clock::now();
    }

    void Time::update() {
        auto currentTime = std::chrono::steady_clock::now();
        std::chrono::duration<float> elapsed = currentTime - _lastTime;
        _lastTime = currentTime;
        advance(elapsed.count());
    }

    void Time::advance(float rawDeltaSeconds) {
        _rawDeltaTime = rawDeltaSeconds;

        // Protección contra saltos grandes de tiempo (ej: mover ventana)
        if (_rawDeltaTime > kMaxDeltaTime) _rawDeltaTime = kMaxDeltaTime;
        if (_rawDeltaTime < 0.0f) _rawDeltaTime = 0.0f;

        _deltaTime = _rawDeltaTime * _timeScale;
    }

    float Time::getDeltaTime() { return _deltaTime; }
    float Time::getUnscaledDeltaTime() { return _rawDeltaTime; }
    float Time::getTimeScale() { return _timeScale; }
    void Time::setTimeScale(float scale) { _timeScale = scale; }
}
