#pragma once

#include <mge/core/Time.h>
#include <mge/core/ecs/Entity.h>
#include <mge/core/ecs/Transform.h>
#include <mge/core/ecs/World.h>
#include <mge/core/math/Geometry.h>
#include <mge/physics/GridQuery.h>

#include <glm/glm.hpp>

namespace mge {

    enum class CharacterMode {
        Walk,
        Fly
    };

    // Movement intent (translated by the game from its actions; does not read keys directly)
    struct CharacterInput {
        float moveForward = 0.0f; // -1..1
        float moveRight = 0.0f;   // -1..1
        float moveUp = 0.0f;      // -1..1, Fly mode only
        bool jump = false;        // Walk: jumps when grounded (holding = jump on landing)
        bool sprint = false;
    };

    // ECS component. Transform::position = character's feet (center of the box base).
    struct CharacterController {
        CharacterMode mode = CharacterMode::Walk;
        bool enabled = true; // false = skip simulation (e.g. chunk underfoot is not loaded yet)
        glm::vec3 halfExtents{0.3f, 0.9f, 0.3f};
        float eyeHeight = 1.62f; // Above feet
        float walkSpeed = 4.3f;
        float sprintMultiplier = 1.5f;
        float flySpeed = 10.0f;
        float flySprintMultiplier = 2.5f;
        float jumpSpeed = 8.0f; // ~1.25 blocks jump height with gravity 28
        float gravity = 28.0f;
        float maxFallSpeed = 60.0f;
        float stepHeight = 0.6f;    // Walk mode and grounded only
        float acceleration = 40.0f; // Towards target velocity on the XZ plane (ground); uses 1/4 in air
        float yaw = -90.0f;         // Degrees; -90 = looking along -Z (same as FlyController)
        float pitch = 0.0f;         // Degrees, ±89
        CharacterInput input;       // Written by the game each frame
        // --- State (written by the system) ---
        glm::vec3 velocity{0.0f};
        bool grounded = false;
        glm::vec3 previousPosition{
            0.0f}; // Position at the start of the last fixed step (for camera interpolation)

        void look(float deltaYawDegrees, float deltaPitchDegrees); // Clamps pitch to ±89
        glm::vec3 forward() const;                                 // View direction (with pitch)
        glm::vec3 flatForward() const; // Without pitch, normalized on XZ (for walking)
        glm::vec3 right() const;
        void setMode(CharacterMode newMode); // Resets velocity.y = 0 when switching to Fly
        void toggleMode();
    };

    // Computes the AABB occupied by the character from their feet position
    inline Aabb characterAabb(const glm::vec3& feet, const glm::vec3& halfExtents) {
        return Aabb(feet - glm::vec3(halfExtents.x, 0.0f, halfExtents.z),
                    feet + glm::vec3(halfExtents.x, 2.0f * halfExtents.y, halfExtents.z));
    }

    // Computes the feet position from the AABB
    inline glm::vec3 characterFeet(const Aabb& box) {
        return glm::vec3((box.min.x + box.max.x) * 0.5f, box.min.y, (box.min.z + box.max.z) * 0.5f);
    }

    // Computes the interpolated eye position of the character
    inline glm::vec3 characterEyePosition(const CharacterController& controller, const Transform& transform,
                                          float alpha) {
        return glm::mix(controller.previousPosition, transform.position, alpha) +
               glm::vec3(0.0f, controller.eyeHeight, 0.0f);
    }

    // One fixed step for a character: pure, testable without ECS.
    glm::vec3 stepCharacter(CharacterController& controller, const glm::vec3& feet, float dt,
                            const SolidQuery& isSolid);
    void stepCharacter(CharacterController& controller, Transform& transform, float dt,
                       const SolidQuery& isSolid);

    // Registers the system in SystemPhase::Fixed: runs stepCharacter for every entity with
    // <CharacterController, Transform>.
    void addCharacterSystem(World& world, SolidQuery isSolid);

    // Component for the camera entity: follows a character at eye height.
    struct CharacterCamera {
        Entity target;
    };

    // Registers in SystemPhase::Render: for each <CharacterCamera, Transform> whose target has
    // CharacterController and Transform, sets position = mix(previousPosition, position, time.alpha()) + (0,
    // eyeHeight, 0) and rotation facing forward() (glm::quatLookAt(forward, up)). `time` must outlive
    // the World.
    void addCharacterCameraSystem(World& world, const Time& time);

} // namespace mge
