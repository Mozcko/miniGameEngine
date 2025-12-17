#pragma once
#include "Camera3D.h"
#include <functional>

namespace Engine {

    class FirstPersonCamera : public Camera3D {
    public:
        FirstPersonCamera(int width, int height, glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f));
        
        // Sobrescribimos el update para añadir físicas
        void update(float deltaTime) override;
        
        // Configurar callback de colisiones
        void setCollisionCallback(std::function<bool(const glm::vec3& min, const glm::vec3& max)> callback) {
            m_CollisionCallback = callback;
        }

    private:
        glm::vec3 m_Velocity;
        bool m_IsGrounded;
        std::function<bool(const glm::vec3&, const glm::vec3&)> m_CollisionCallback;
    };
}