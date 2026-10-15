#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <functional>

namespace mge {

    class Camera3D {
    public:
        // Constructor
        Camera3D(int width, int height, glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f));

        // Procesa input (teclado/ratón) y actualiza la posición/vista
        virtual void update(float deltaTime);

        // Actualiza la matriz de proyección (al redimensionar ventana)
        void updateProjection(float width, float height);

        // Getters
        glm::mat4 getViewMatrix() const;
        glm::mat4 getProjectionMatrix() const;
        glm::vec3 getPosition() const { return m_position; }
        void setPosition(const glm::vec3& pos) { m_position = pos; }

        // Frustum Culling (Simplificado o placeholder para mantener compatibilidad)
        bool isSphereVisible(const glm::vec3& center, float radius) const;

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