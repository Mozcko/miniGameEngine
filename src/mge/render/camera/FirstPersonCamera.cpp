#include <mge/render/camera/FirstPersonCamera.h>
#include <mge/platform/InputManager.h>
#include <SDL3/SDL.h>
#include <cmath>

namespace mge {

    FirstPersonCamera::FirstPersonCamera(int width, int height, glm::vec3 position)
        : Camera3D(width, height, position), m_velocity(0.0f), m_isGrounded(false) {}

    void FirstPersonCamera::update(float deltaTime) {
        if (deltaTime <= 0.0001f) return;

        // 1. Ratón (Mirar)
        glm::vec2 mouseDelta = InputManager::getMouseDelta();
        if (mouseDelta.x != 0.0f || mouseDelta.y != 0.0f) {
            m_yaw += mouseDelta.x * m_mouseSensitivity;
            m_pitch -= mouseDelta.y * m_mouseSensitivity;
            if (m_pitch > 89.0f) m_pitch = 89.0f;
            if (m_pitch < -89.0f) m_pitch = -89.0f;
            updateCameraVectors();
        }

        // 2. Teclado (Movimiento Horizontal)
        float speed = m_movementSpeed * deltaTime;
        if (InputManager::isKeyPressed(SDL_SCANCODE_LCTRL)) speed *= 1.5f; // Sprint

        glm::vec3 moveDir(0.0f);
        // Proyectamos Front en el plano XZ para no volar al mirar arriba
        glm::vec3 flatFront = glm::normalize(glm::vec3(m_front.x, 0.0f, m_front.z));
        glm::vec3 flatRight = glm::normalize(glm::vec3(m_right.x, 0.0f, m_right.z));

        if (InputManager::isKeyPressed(SDL_SCANCODE_W)) moveDir += flatFront;
        if (InputManager::isKeyPressed(SDL_SCANCODE_S)) moveDir -= flatFront;
        if (InputManager::isKeyPressed(SDL_SCANCODE_A)) moveDir -= flatRight;
        if (InputManager::isKeyPressed(SDL_SCANCODE_D)) moveDir += flatRight;

        if (glm::length(moveDir) > 0.0f) {
            moveDir = glm::normalize(moveDir) * speed;
        }

        // Salto
        if (m_isGrounded && InputManager::isKeyPressed(SDL_SCANCODE_SPACE)) {
            m_velocity.y = 6.5f;
            m_isGrounded = false;
        }

        // 3. Físicas y Colisiones
        if (!m_collisionCallback) {
            // Modo espectador si no hay callback
            m_position += moveDir;
            m_position.y += m_velocity.y * deltaTime;
        } else {
            // Colisión X
            m_position.x += moveDir.x;
            if (m_collisionCallback(m_position - glm::vec3(0.3f, 1.5f, 0.3f),
                                    m_position + glm::vec3(0.3f, 0.3f, 0.3f))) {
                m_position.x -= moveDir.x;
            }

            // Colisión Z
            m_position.z += moveDir.z;
            if (m_collisionCallback(m_position - glm::vec3(0.3f, 1.5f, 0.3f),
                                    m_position + glm::vec3(0.3f, 0.3f, 0.3f))) {
                m_position.z -= moveDir.z;
            }

            // Gravedad y Colisión Y
            m_velocity.y -= 18.0f * deltaTime; // Gravedad
            float dy = m_velocity.y * deltaTime;

            m_position.y += dy;
            glm::vec3 min = m_position - glm::vec3(0.3f, 1.5f, 0.3f);
            glm::vec3 max = m_position + glm::vec3(0.3f, 0.3f, 0.3f);

            if (m_collisionCallback(min, max)) {
                if (m_velocity.y < 0) { // Suelo
                    m_isGrounded = true;
                    m_velocity.y = 0;
                    // Ajuste para no atravesar el suelo (Snap al bloque)
                    float blockTop = std::floor(min.y + 0.5f) + 0.5f;
                    m_position.y = blockTop + 1.5f + 0.001f;
                } else { // Techo
                    m_velocity.y = 0;
                    m_position.y -= dy;
                }
            } else {
                m_isGrounded = false;
            }
        }

        // Actualizar matriz de vista
        m_viewMatrix = glm::lookAt(m_position, m_position + m_front, m_up);
    }
}