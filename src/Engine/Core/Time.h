#pragma once
#include <SDL2/SDL.h>

namespace MiniGameEngine {
    class Time {
    public:
        static void init();
        static void update();
        
        static float getDeltaTime();      // Tiempo escalado (afectado por pausa/slowmo)
        static float getUnscaledDeltaTime(); // Tiempo real (para UI, inputs sistema)
        static float getTimeScale();
        static void setTimeScale(float scale);

    private:
        static Uint64 _lastTime;
        static float _deltaTime;
        static float _rawDeltaTime;
        static float _timeScale;
    };
}