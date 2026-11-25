#pragma once
#include <cstdint>

namespace mge {

    // Time state of an Application (app.time()). Updated by the loop; read by the game.
    // Fixed timestep: onFixedUpdate runs at tickRate with fixedDelta(); onUpdate receives delta() (variable).
    class Time {
    public:
        // --- Current frame (variable) ---
        float delta() const { return m_delta; }                 // Scaled (0 when paused)
        float unscaledDelta() const { return m_unscaledDelta; } // Real (UI, animations while paused)

        // --- Fixed timestep ---
        float fixedDelta() const { return m_fixedDelta; }
        // Fraction [0, 1) of the next fixed step already elapsed: for render interpolation
        float alpha() const { return m_alpha; }

        // --- Totals ---
        double elapsed() const { return m_elapsed; }                 // Scaled
        double unscaledElapsed() const { return m_unscaledElapsed; } // Real
        std::uint64_t frameCount() const { return m_frameCount; }
        std::uint64_t fixedStepCount() const { return m_fixedStepCount; }

        // --- Scale: 1 = normal, 0 = paused, 0.5 = slow motion ---
        float timeScale() const { return m_timeScale; }
        void setTimeScale(float scale) { m_timeScale = scale < 0.0f ? 0.0f : scale; }

    private:
        friend class Application; // Only the loop writes the state

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
