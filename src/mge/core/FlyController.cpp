#include <mge/core/FlyController.h>

#include <algorithm>
#include <cmath>

#include <glm/gtc/quaternion.hpp>

namespace mge {

    FlyController::FlyController(const glm::vec3& position, float yawDegrees, float pitchDegrees)
        : m_position(position), m_yaw(yawDegrees), m_pitch(std::clamp(pitchDegrees, -89.0f, 89.0f)) {}

    glm::vec3 FlyController::forward() const {
        const float yaw = glm::radians(m_yaw);
        const float pitch = glm::radians(m_pitch);
        return glm::normalize(
            glm::vec3(std::cos(yaw) * std::cos(pitch), std::sin(pitch), std::sin(yaw) * std::cos(pitch)));
    }

    glm::vec3 FlyController::right() const {
        return glm::normalize(glm::cross(forward(), glm::vec3(0.0f, 1.0f, 0.0f)));
    }

    bool FlyController::collides() const {
        return m_collision && m_collision(m_position - halfExtents, m_position + halfExtents);
    }

    void FlyController::update(float dt, const CameraInput& input) {
        if (dt <= 0.0f) return; // Paused

        // Look (screen: y points down → pitch up when moving the mouse up)
        m_yaw += input.lookX * sensitivity;
        m_pitch = std::clamp(m_pitch - input.lookY * sensitivity, -89.0f, 89.0f);

        glm::vec3 move = forward() * input.moveForward + right() * input.moveRight +
                         glm::vec3(0.0f, 1.0f, 0.0f) * input.moveUp;
        if (glm::length(move) <= 0.0f) return;
        move = glm::normalize(move) * speed * (input.sprint ? sprintMultiplier : 1.0f) * dt;

        // Axis by axis (X, Z, Y): allows sliding along a wall
        for (int axis : {0, 2, 1}) {
            m_position[axis] += move[axis];
            if (collides()) m_position[axis] -= move[axis];
        }
    }

    void FlyController::apply(Transform& transform) const {
        transform.position = m_position;
        // quatLookAt (right-handed): rotation mapping local -Z to forward()
        transform.rotation = glm::quatLookAt(forward(), glm::vec3(0.0f, 1.0f, 0.0f));
    }
}
