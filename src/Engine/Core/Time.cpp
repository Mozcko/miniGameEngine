#include "Time.h"

namespace MiniGameEngine {
    Uint64 Time::_lastTime = 0;
    float Time::_deltaTime = 0.0f;
    float Time::_rawDeltaTime = 0.0f;
    float Time::_timeScale = 1.0f;

    void Time::init() {
        _lastTime = SDL_GetPerformanceCounter();
    }

    void Time::update() {
        Uint64 currentTime = SDL_GetPerformanceCounter();
        _rawDeltaTime = (float)(currentTime - _lastTime) / (float)SDL_GetPerformanceFrequency();
        _lastTime = currentTime;
        
        // Protección contra saltos grandes de tiempo (ej: mover ventana)
        if (_rawDeltaTime > 0.1f) _rawDeltaTime = 0.1f;

        _deltaTime = _rawDeltaTime * _timeScale;
    }

    float Time::getDeltaTime() { return _deltaTime; }
    float Time::getUnscaledDeltaTime() { return _rawDeltaTime; }
    float Time::getTimeScale() { return _timeScale; }
    void Time::setTimeScale(float scale) { _timeScale = scale; }
}