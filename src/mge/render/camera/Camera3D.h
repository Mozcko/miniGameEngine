#pragma once

#include <mge/core/math/Geometry.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <functional>

namespace mge {

    // Intención de movimiento para las cámaras, ya traducida desde el input del juego
    // (acciones/ejes). Las cámaras no leen teclas: así no dependen de la plataforma.
    struct CameraInput {
        float moveForward = 0.0f; // -1..1 (adelante +)
        float moveRight = 0.0f;   // -1..1 (derecha +)
        float moveUp = 0.0f;      // -1..1 (arriba +). Solo en vuelo libre (Camera3D)
        bool sprint = false;
        bool jump = false;  // Solo FirstPersonCamera
        float lookX = 0.0f; // Movimiento del ratón en píxeles (derecha +)
        float lookY = 0.0f; // Movimiento del ratón en píxeles (abajo +, como en pantalla)
    };

    class Camera3D {
    public:
        // Constructor
        Camera3D(int width, int height, glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f));

        // Procesa input (teclado/ratón) y actualiza la posición/vista
        virtual void update(float deltaTime, const CameraInput& input);

        // Actualiza la matriz de proyección (al redimensionar ventana)
        void updateProjection(float width, float height);

        // Getters
        glm::mat4 getViewMatrix() const;
        glm::mat4 getProjectionMatrix() const;
        glm::vec3 getPosition() const { return m_position; }
        void setPosition(const glm::vec3& pos) { m_position = pos; }

        // Frustum culling con los planos de projection * view (actualizados en update/updateProjection)
        Frustum getFrustum() const { return Frustum::fromViewProjection(m_projectionMatrix * m_viewMatrix); }
        bool isSphereVisible(const glm::vec3& center, float radius) const;
        bool isAabbVisible(const Aabb& box) const;

        // Configuración
        void setSpeed(float speed) { m_movementSpeed = speed; }
        void setSensitivity(float sensitivity) { m_mouseSensitivity = sensitivity; }

        void setCollisionCallback(std::function<bool(const glm::vec3& min, const glm::vec3& max)> callback) {
            m_collisionCallback = callback;
        }

    protected:
        void updateCameraVectors();

        // Atributos de la cámara
        glm::vec3 m_position;
        glm::vec3 m_front;
        glm::vec3 m_up;
        glm::vec3 m_right;
        glm::vec3 m_worldUp;

        // Ángulos de Euler
        float m_yaw;
        float m_pitch;

        // Opciones de movimiento
        float m_movementSpeed;
        float m_mouseSensitivity;

        // Proyección
        float m_width, m_height;
        float m_fov;
        glm::mat4 m_projectionMatrix;
        glm::mat4 m_viewMatrix;

        // Physics
        glm::vec3 m_velocity;
        bool m_isGrounded;
        std::function<bool(const glm::vec3&, const glm::vec3&)> m_collisionCallback;
    };
}