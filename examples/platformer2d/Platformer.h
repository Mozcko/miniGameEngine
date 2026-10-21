#pragma once

#include <examples/platformer2d/Level.h>

#include <vector>

#include <glm/glm.hpp>

namespace platformer {

    // Lo que el jugador quiere hacer en este paso
    struct PlayerInput {
        float move = 0.0f; // -1 izquierda .. 1 derecha
        bool jump = false; // Mientras esté apretado (salta al tocar el piso)
    };

    // Lo que pasó en un paso (para sonidos y cambios de nivel)
    struct StepEvents {
        int coins = 0;
        bool jumped = false;
        bool died = false;     // Cayó al vacío o tocó un enemigo: vuelve al inicio
        bool finished = false; // Llegó al final del nivel
    };

    // Simulación del plataformero (headless, testeable): un jugador caja contra las casillas sólidas del
    // nivel. Unidades: casillas; y hacia abajo, como las filas del archivo. Paso fijo (onFixedUpdate).
    class Platformer {
    public:
        static constexpr glm::vec2 kPlayerSize{0.75f, 0.9f};
        static constexpr float kRunSpeed = 7.0f;   // casillas/s
        static constexpr float kJumpSpeed = 13.0f; // casillas/s (≈ 2.4 casillas de altura)
        static constexpr float kGravity = 35.0f;   // casillas/s²
        static constexpr float kMaxFallSpeed = 20.0f;

        explicit Platformer(Level level);

        StepEvents step(const PlayerInput& input, float dt);
        void respawn();

        const Level& level() const { return m_level; }
        glm::vec2 playerPosition() const { return m_position; } // Esquina de arriba a la izquierda
        glm::vec2 playerVelocity() const { return m_velocity; }
        bool grounded() const { return m_grounded; }
        bool collected(std::size_t item) const { return m_collected[item]; }
        int coins() const { return m_coins; }
        int deaths() const { return m_deaths; }

    private:
        // ¿La caja (esquina, tamaño) toca alguna casilla sólida?
        bool overlapsSolid(const glm::vec2& pos) const;
        // Mueve en un eje y frena contra las casillas; true si chocó
        bool moveAxis(int axis, float delta);

        Level m_level;
        glm::vec2 m_position{0.0f};
        glm::vec2 m_velocity{0.0f};
        bool m_grounded = false;
        std::vector<bool> m_collected;
        int m_coins = 0;
        int m_deaths = 0;
    };
}
