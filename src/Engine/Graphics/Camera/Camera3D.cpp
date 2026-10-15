#include "Camera3D.h"
#include "../../Input/InputManager.h" // Asegúrate de que la ruta sea correcta
#include <SDL2/SDL.h>
#include <cmath>

namespace Engine {

    Camera3D::Camera3D(int width, int height, glm::vec3 position)
        : Front(glm::vec3(0.0f, 0.0f, -1.0f)), MovementSpeed(10.0f), MouseSensitivity(0.1f),
          _width((float)width), _height((float)height), _fov(45.0f),
          m_Velocity(0.0f), m_IsGrounded(false)
    {
        Position = position;
        WorldUp = glm::vec3(0.0f, 1.0f, 0.0f);
        Yaw = -90.0f;
        Pitch = 0.0f;
        
        updateCameraVectors();
        updateProjection(_width, _height);
    }

    void Camera3D::update(float deltaTime) {
        // Si deltaTime es 0 (juego pausado), no procesamos movimiento
        if (deltaTime <= 0.0001f) return;

        // 1. Ratón
        if (InputManager::IsCursorLocked()) {
            glm::vec2 mouseDelta = InputManager::GetMouseDelta();
            if (mouseDelta.x != 0.0f || mouseDelta.y != 0.0f) {
                Yaw   += mouseDelta.x * MouseSensitivity;
                Pitch -= mouseDelta.y * MouseSensitivity; // Invertido (SDL Y es abajo)

                if (Pitch > 89.0f) Pitch = 89.0f;
                if (Pitch < -89.0f) Pitch = -89.0f;

                updateCameraVectors();
            }
        }

        // 2. Teclado
        float speed = MovementSpeed * deltaTime;
        
        // Sprint con Control
        if (InputManager::IsKeyPressed(SDL_SCANCODE_LCTRL)) {
            speed *= 2.5f;
        }

        glm::vec3 moveDir(0.0f);
        
        // Movimiento libre (Free Fly)
        if (InputManager::IsKeyPressed(SDL_SCANCODE_W)) moveDir += Front;
        if (InputManager::IsKeyPressed(SDL_SCANCODE_S)) moveDir -= Front;
        if (InputManager::IsKeyPressed(SDL_SCANCODE_A)) moveDir -= Right;
        if (InputManager::IsKeyPressed(SDL_SCANCODE_D)) moveDir += Right;
        if (InputManager::IsKeyPressed(SDL_SCANCODE_SPACE)) moveDir += WorldUp;
        if (InputManager::IsKeyPressed(SDL_SCANCODE_LSHIFT)) moveDir -= WorldUp;

        if (glm::length(moveDir) > 0.0f) moveDir = glm::normalize(moveDir) * speed;

        if (m_CollisionCallback) {
            // Colisiones sin gravedad (Flotar)
            glm::vec3 boxSize(0.3f); // Tamaño de la caja de colisión
            
            // Eje X
            Position.x += moveDir.x;
            if (m_CollisionCallback(Position - boxSize, Position + boxSize)) Position.x -= moveDir.x;

            // Eje Z
            Position.z += moveDir.z;
            if (m_CollisionCallback(Position - boxSize, Position + boxSize)) Position.z -= moveDir.z;

            // Eje Y
            Position.y += moveDir.y;
            if (m_CollisionCallback(Position - boxSize, Position + boxSize)) Position.y -= moveDir.y;
            
        } else {
            // Sin colisiones
            Position += moveDir;
        }

        // Recalcular vista si hubo movimiento (opcional optimizar checkeando cambios)
        _viewMatrix = glm::lookAt(Position, Position + Front, Up);
    }

    void Camera3D::updateProjection(float width, float height) {
        _width = width;
        _height = height;
        _projectionMatrix = glm::perspective(glm::radians(_fov), _width / _height, 0.1f, 1000.0f);
    }

    void Camera3D::updateCameraVectors() {
        glm::vec3 front;
        front.x = cos(glm::radians(Yaw)) * cos(glm::radians(Pitch));
        front.y = sin(glm::radians(Pitch));
        front.z = sin(glm::radians(Yaw)) * cos(glm::radians(Pitch));
        Front = glm::normalize(front);
        Right = glm::normalize(glm::cross(Front, WorldUp));
        Up    = glm::normalize(glm::cross(Right, Front));
    }

    glm::mat4 Camera3D::getViewMatrix() const {
        return _viewMatrix;
    }

    glm::mat4 Camera3D::getProjectionMatrix() const {
        return _projectionMatrix;
    }

    bool Camera3D::isSphereVisible(const glm::vec3& /*center*/, float /*radius*/) const {
        return true; // Placeholder: Implementar extracción de planos del frustum si es necesario
    }
}