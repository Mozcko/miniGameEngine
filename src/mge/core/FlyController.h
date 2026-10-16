#pragma once

#include <mge/core/ecs/Transform.h>

#include <functional>

#include <glm/glm.hpp>

namespace mge {

    // Intención de movimiento para controladores de cámara, ya traducida desde el input del juego
    // (acciones/ejes). Los controladores no leen teclas: así no dependen de la plataforma.
    struct CameraInput {
        float moveForward = 0.0f; // -1..1 (adelante +)
        float moveRight = 0.0f;   // -1..1 (derecha +)
        float moveUp = 0.0f;      // -1..1 (arriba +)
        bool sprint = false;
        float lookX = 0.0f; // Movimiento del ratón en píxeles (derecha +)
        float lookY = 0.0f; // Movimiento del ratón en píxeles (abajo +, como en pantalla)
    };

    // Cámara de vuelo libre (estilo espectador): yaw/pitch con el ratón, movimiento en la dirección de la
    // vista, colisiones opcionales eje por eje contra una caja alrededor de la posición. Escribe un
    // Transform (posición + rotación) para el componente Camera.
    class FlyController {
    public:
        // ¿La caja [min, max] choca con algo? (ej: bloques sólidos de un mundo voxel)
        using CollisionCallback = std::function<bool(const glm::vec3& min, const glm::vec3& max)>;

        explicit FlyController(const glm::vec3& position = glm::vec3(0.0f), float yawDegrees = -90.0f,
                               float pitchDegrees = 0.0f);

        // dt <= 0 (pausa) no mueve ni gira
        void update(float dt, const CameraInput& input);
        void apply(Transform& transform) const; // Posición + rotación (mira hacia forward())

        const glm::vec3& position() const { return m_position; }
        void setPosition(const glm::vec3& position) { m_position = position; }
        float yaw() const { return m_yaw; }     // Grados; -90 = mira hacia -Z
        float pitch() const { return m_pitch; } // Grados, limitado a ±89
        glm::vec3 forward() const;
        glm::vec3 right() const;

        void setCollisionCallback(CollisionCallback callback) { m_collision = std::move(callback); }
        float speed = 10.0f; // Unidades por segundo
        float sprintMultiplier = 2.5f;
        float sensitivity = 0.1f;    // Grados por píxel de ratón
        glm::vec3 halfExtents{0.3f}; // Caja de colisión alrededor de la posición

    private:
        bool collides() const;

        glm::vec3 m_position;
        float m_yaw;
        float m_pitch;
        CollisionCallback m_collision;
    };
}
