#include <mge/render/camera/Camera3D.h>
#include <mge/platform/InputManager.h> // Asegúrate de que la ruta sea correcta
#include <SDL3/SDL.h>
#include <cmath>

namespace mge {

    Camera3D::Camera3D(int width, int height, glm::vec3 position)
        : m_front(glm::vec3(0.0f, 0.0f, -1.0f)), m_movementSpeed(10.0f), m_mouseSensitivity(0.1f),
          m_width((float)width), m_height((float)height), m_fov(45.0f), m_velocity(0.0f),
          m_isGrounded(false) {
        m_position = position;
        m_worldUp = glm::vec3(0.0f, 1.0f, 0.0f);
        m_yaw = -90.0f;
        m_pitch = 0.0f;

        updateCameraVectors();
        updateProjection(m_width, m_height);
    }

    void Camera3D::update(float deltaTime) {
        // Si deltaTime es 0 (juego pausado), no procesamos movimiento
        if (deltaTime <= 0.0001f) return;

        // 1. Ratón
        if (InputManager::isCursorLocked()) {
            glm::vec2 mouseDelta = InputManager::getMouseDelta();
            if (mouseDelta.x != 0.0f || mouseDelta.y != 0.0f) {
                m_yaw += mouseDelta.x * m_mouseSensitivity;
                m_pitch -= mouseDelta.y * m_mouseSensitivity; // Invertido (SDL Y es abajo)

                if (m_pitch > 89.0f) m_pitch = 89.0f;
                if (m_pitch < -89.0f) m_pitch = -89.0f;

                updateCameraVectors();
            }
        }

        // 2. Teclado
        float speed = m_movementSpeed * deltaTime;

        // Sprint con Control
        if (InputManager::isKeyPressed(SDL_SCANCODE_LCTRL)) {
            speed *= 2.5f;
        }

        glm::vec3 moveDir(0.0f);

        // Movimiento libre (Free Fly)
        if (InputManager::isKeyPressed(SDL_SCANCODE_W)) moveDir += m_front;
        if (InputManager::isKeyPressed(SDL_SCANCODE_S)) moveDir -= m_front;
        if (InputManager::isKeyPressed(SDL_SCANCODE_A)) moveDir -= m_right;
        if (InputManager::isKeyPressed(SDL_SCANCODE_D)) moveDir += m_right;
        if (InputManager::isKeyPressed(SDL_SCANCODE_SPACE)) moveDir += m_worldUp;
        if (InputManager::isKeyPressed(SDL_SCANCODE_LSHIFT)) moveDir -= m_worldUp;

        if (glm::length(moveDir) > 0.0f) moveDir = glm::normalize(moveDir) * speed;

        if (m_collisionCallback) {
            // Colisiones sin gravedad (Flotar)
            glm::vec3 boxSize(0.3f); // Tamaño de la caja de colisión

            // Eje X
            m_position.x += moveDir.x;
            if (m_collisionCallback(m_position - boxSize, m_position + boxSize)) m_position.x -= moveDir.x;

            // Eje Z
            m_position.z += moveDir.z;
            if (m_collisionCallback(m_position - boxSize, m_position + boxSize)) m_position.z -= moveDir.z;

            // Eje Y
            m_position.y += moveDir.y;
            if (m_collisionCallback(m_position - boxSize, m_position + boxSize)) m_position.y -= moveDir.y;

        } else {
            // Sin colisiones
            m_position += moveDir;
        }

        // Recalcular vista si hubo movimiento (opcional optimizar checkeando cambios)
        m_viewMatrix = glm::lookAt(m_position, m_position + m_front, m_up);
    }

    void Camera3D::updateProjection(float width, float height) {
        m_width = width;
        m_height = height;
        m_projectionMatrix = glm::perspective(glm::radians(m_fov), m_width / m_height, 0.1f, 1000.0f);
    }

    void Camera3D::updateCameraVectors() {
        glm::vec3 front;
        front.x = cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
        front.y = sin(glm::radians(m_pitch));
        front.z = sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
        m_front = glm::normalize(front);
        m_right = glm::normalize(glm::cross(m_front, m_worldUp));
        m_up = glm::normalize(glm::cross(m_right, m_front));
    }

    glm::mat4 Camera3D::getViewMatrix() const {
        return m_viewMatrix;
    }

    glm::mat4 Camera3D::getProjectionMatrix() const {
        return m_projectionMatrix;
    }

    bool Camera3D::isSphereVisible(const glm::vec3& /*center*/, float /*radius*/) const {
        return true; // Placeholder: Implementar extracción de planos del frustum si es necesario
    }
}