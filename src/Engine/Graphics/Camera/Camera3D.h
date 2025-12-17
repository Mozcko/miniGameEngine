#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <functional>

namespace Engine {

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
        glm::vec3 getPosition() const { return Position; }
        void setPosition(const glm::vec3& pos) { Position = pos; }
        
        // Frustum Culling (Simplificado o placeholder para mantener compatibilidad)
        bool isSphereVisible(const glm::vec3& center, float radius) const;

        // Configuración
        void setSpeed(float speed) { MovementSpeed = speed; }
        void setSensitivity(float sensitivity) { MouseSensitivity = sensitivity; }
        
        void setCollisionCallback(std::function<bool(const glm::vec3& min, const glm::vec3& max)> callback) {
            m_CollisionCallback = callback;
        }

    protected:
        void updateCameraVectors();

        // Atributos de la cámara
        glm::vec3 Position;
        glm::vec3 Front;
        glm::vec3 Up;
        glm::vec3 Right;
        glm::vec3 WorldUp;

        // Ángulos de Euler
        float Yaw;
        float Pitch;

        // Opciones de movimiento
        float MovementSpeed;
        float MouseSensitivity;

        // Proyección
        float _width, _height;
        float _fov;
        glm::mat4 _projectionMatrix;
        glm::mat4 _viewMatrix;

        // Physics
        glm::vec3 m_Velocity;
        bool m_IsGrounded;
        std::function<bool(const glm::vec3&, const glm::vec3&)> m_CollisionCallback;
    };
}