#pragma once
#include <cstdint>

namespace mge {

    // Estado de tiempo de una Application (app.time()). Lo actualiza el loop; el juego lo lee.
    // Paso fijo: onFixedUpdate corre a tickRate con fixedDelta(); onUpdate recibe delta() (variable).
    class Time {
    public:
        // --- Frame actual (variable) ---
        float delta() const { return m_delta; }                 // Escalado (0 en pausa)
        float unscaledDelta() const { return m_unscaledDelta; } // Real (UI, animaciones en pausa)

        // --- Paso fijo ---
        float fixedDelta() const { return m_fixedDelta; }
        // Fracción [0, 1) del siguiente paso fijo ya transcurrida: para interpolar el render
        float alpha() const { return m_alpha; }

        // --- Totales ---
        double elapsed() const { return m_elapsed; }                 // Escalado
        double unscaledElapsed() const { return m_unscaledElapsed; } // Real
        std::uint64_t frameCount() const { return m_frameCount; }
        std::uint64_t fixedStepCount() const { return m_fixedStepCount; }

        // --- Escala: 1 = normal, 0 = pausa, 0.5 = cámara lenta ---
        float timeScale() const { return m_timeScale; }
        void setTimeScale(float scale) { m_timeScale = scale < 0.0f ? 0.0f : scale; }

    private:
        friend class Application; // Solo el loop escribe el estado

        float m_delta = 0.0f;
        float m_unscaledDelta = 0.0f;
        float m_fixedDelta = 1.0f / 60.0f;
        float m_alpha = 0.0f;
        double m_elapsed = 0.0;
        double m_unscaledElapsed = 0.0;
        double m_accumulator = 0.0;
        std::uint64_t m_frameCount = 0;
        std::uint64_t m_fixedStepCount = 0;
        float m_timeScale = 1.0f;
    };
}
