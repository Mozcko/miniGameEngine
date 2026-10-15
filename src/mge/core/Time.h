#pragma once
#include <chrono>

namespace MiniGameEngine {
    class Time {
    public:
        static void init();
        static void update();

        // Aplica un delta "crudo" (segundos): recorta saltos grandes y aplica la escala.
        // update() lo usa con el reloj real; los tests lo llaman directamente.
        static void advance(float rawDeltaSeconds);

        static float getDeltaTime();      // Tiempo escalado (afectado por pausa/slowmo)
        static float getUnscaledDeltaTime(); // Tiempo real (para UI, inputs sistema)
        static float getTimeScale();
        static void setTimeScale(float scale);

        // Delta máximo por frame (protege contra pausas largas, ej: mover la ventana)
        static constexpr float kMaxDeltaTime = 0.1f;

    private:
        static std::chrono::steady_clock::time_point _lastTime;
        static float _deltaTime;
        static float _rawDeltaTime;
        static float _timeScale;
    };
}
