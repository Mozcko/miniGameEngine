#pragma once

#include <mge/core/ecs/Transform.h>

#include <functional>

#include <glm/glm.hpp>

namespace mge {

    // Movement intent for camera controllers, already translated from game input
    // (actions/axes). Controllers do not read keys so they stay platform-independent.
    struct CameraInput {
        float moveForward = 0.0f; // -1..1 (forward +)
        float moveRight = 0.0f;   // -1..1 (right +)
        float moveUp = 0.0f;      // -1..1 (up +)
        bool sprint = false;
        float lookX = 0.0f; // Mouse movement in pixels (right +)
        float lookY = 0.0f; // Mouse movement in pixels (down +, as on screen)
    };

    // Free-fly camera (spectator style): yaw/pitch with the mouse, movement along the view direction,
    // optional axis-by-axis collisions against a box around the position. Writes a
    // Transform (position + rotation) for the Camera component.
    class FlyController {
    public:
        // Does the box [min, max] collide with anything? (e.g., solid blocks in a voxel world)
        using CollisionCallback = std::function<bool(const glm::vec3& min, const glm::vec3& max)>;

        explicit FlyController(const glm::vec3& position = glm::vec3(0.0f), float yawDegrees = -90.0f,
                               float pitchDegrees = 0.0f);

        // dt <= 0 (paused) neither moves nor rotates
        void update(float dt, const CameraInput& input);
        void apply(Transform& transform) const; // Position + rotation (looks towards forward())

        const glm::vec3& position() const { return m_position; }
        void setPosition(const glm::vec3& position) { m_position = position; }
        float yaw() const { return m_yaw; }     // Degrees; -90 = looks towards -Z
        float pitch() const { return m_pitch; } // Degrees, clamped to ±89
        glm::vec3 forward() const;
        glm::vec3 right() const;

        void setCollisionCallback(CollisionCallback callback) { m_collision = std::move(callback); }
        float speed = 10.0f; // Units per second
        float sprintMultiplier = 2.5f;
        float sensitivity = 0.1f;    // Degrees per mouse pixel
        glm::vec3 halfExtents{0.3f}; // Collision box around the position

    private:
        bool collides() const;

        glm::vec3 m_position;
        float m_yaw;
        float m_pitch;
        CollisionCallback m_collision;
    };
}
