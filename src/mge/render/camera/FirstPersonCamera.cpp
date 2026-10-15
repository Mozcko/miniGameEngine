#include <mge/render/camera/FirstPersonCamera.h>
#include <mge/platform/InputManager.h>
#include <SDL2/SDL.h>
#include <cmath>

namespace Engine {

    FirstPersonCamera::FirstPersonCamera(int width, int height, glm::vec3 position) 
        : Camera3D(width, height, position), m_Velocity(0.0f), m_IsGrounded(false) 
    {
    }

    void FirstPersonCamera::update(float deltaTime) {
        if (deltaTime <= 0.0001f) return;

        // 1. Ratón (Mirar)
        glm::vec2 mouseDelta = InputManager::GetMouseDelta();
        if (mouseDelta.x != 0.0f || mouseDelta.y != 0.0f) {
            Yaw += mouseDelta.x * MouseSensitivity;
            Pitch -= mouseDelta.y * MouseSensitivity;
            if (Pitch > 89.0f) Pitch = 89.0f;
            if (Pitch < -89.0f) Pitch = -89.0f;
            updateCameraVectors();
        }

        // 2. Teclado (Movimiento Horizontal)
        float speed = MovementSpeed * deltaTime;
        if (InputManager::IsKeyPressed(SDL_SCANCODE_LCTRL)) speed *= 1.5f; // Sprint

        glm::vec3 moveDir(0.0f);
        // Proyectamos Front en el plano XZ para no volar al mirar arriba
        glm::vec3 flatFront = glm::normalize(glm::vec3(Front.x, 0.0f, Front.z));
        glm::vec3 flatRight = glm::normalize(glm::vec3(Right.x, 0.0f, Right.z));

        if (InputManager::IsKeyPressed(SDL_SCANCODE_W)) moveDir += flatFront;
        if (InputManager::IsKeyPressed(SDL_SCANCODE_S)) moveDir -= flatFront;
        if (InputManager::IsKeyPressed(SDL_SCANCODE_A)) moveDir -= flatRight;
        if (InputManager::IsKeyPressed(SDL_SCANCODE_D)) moveDir += flatRight;
        
        if (glm::length(moveDir) > 0.0f) {
            moveDir = glm::normalize(moveDir) * speed;
        }

        // Salto
        if (m_IsGrounded && InputManager::IsKeyPressed(SDL_SCANCODE_SPACE)) {
            m_Velocity.y = 6.5f;
            m_IsGrounded = false;
        }

        // 3. Físicas y Colisiones
        if (!m_CollisionCallback) {
            // Modo espectador si no hay callback
            Position += moveDir;
            Position.y += m_Velocity.y * deltaTime;
        } else {
            // Colisión X
            Position.x += moveDir.x;
            if (m_CollisionCallback(Position - glm::vec3(0.3f, 1.5f, 0.3f), Position + glm::vec3(0.3f, 0.3f, 0.3f))) {
                Position.x -= moveDir.x;
            }

            // Colisión Z
            Position.z += moveDir.z;
            if (m_CollisionCallback(Position - glm::vec3(0.3f, 1.5f, 0.3f), Position + glm::vec3(0.3f, 0.3f, 0.3f))) {
                Position.z -= moveDir.z;
            }

            // Gravedad y Colisión Y
            m_Velocity.y -= 18.0f * deltaTime; // Gravedad
            float dy = m_Velocity.y * deltaTime;
            
            Position.y += dy;
            glm::vec3 min = Position - glm::vec3(0.3f, 1.5f, 0.3f);
            glm::vec3 max = Position + glm::vec3(0.3f, 0.3f, 0.3f);

            if (m_CollisionCallback(min, max)) {
                if (m_Velocity.y < 0) { // Suelo
                    m_IsGrounded = true;
                    m_Velocity.y = 0;
                    // Ajuste para no atravesar el suelo (Snap al bloque)
                    float blockTop = std::floor(min.y + 0.5f) + 0.5f;
                    Position.y = blockTop + 1.5f + 0.001f;
                } else { // Techo
                    m_Velocity.y = 0;
                    Position.y -= dy;
                }
            } else {
                m_IsGrounded = false;
            }
        }
        
        // Actualizar matriz de vista
        _viewMatrix = glm::lookAt(Position, Position + Front, Up);
    }
}