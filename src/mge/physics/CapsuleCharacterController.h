#pragma once

#include <mge/core/Time.h>
#include <mge/core/ecs/Entity.h>
#include <mge/core/ecs/Transform.h>
#include <mge/core/ecs/World.h>
#include <mge/core/math/Geometry.h>
#include <mge/physics/CharacterController.h>
#include <mge/physics/MeshCollider.h>

#include <glm/glm.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace mge {

    struct CapsuleMoveOptions {
        float maxSlopeAngle = 45.0f;      // Maximum walkable slope angle in degrees
        float stepHeight = 0.5f;          // Maximum auto-climb step height (0 = disabled)
        float groundSnapDistance = 0.35f; // Maximum downward snap distance when leaving ground (0 = disabled)
        float skin = 1e-3f;               // Separation kept from mesh surfaces
        bool wasGrounded = false;         // True if grounded at the start of the step
        bool allowStepping = true;
        bool allowGroundSnap = true;
        bool flyMode = false; // 3D collide-and-slide without slope/step/snap restrictions
    };

    struct CapsuleMoveResult {
        glm::vec3 feet{0.0f};
        glm::vec3 moved{0.0f};
        glm::vec3 groundNormal{0.0f, 1.0f, 0.0f};
        glm::vec3 slideNormal{0.0f, 1.0f, 0.0f}; // Contact normal when sliding on a steep slope
        bool grounded = false;
        bool onSteepSlope = false;
        bool hitWall = false;
        bool hitCeiling = false;
        bool stepped = false;
        bool snapped = false;
    };

    // Moves an upright capsule by `delta` against a static MeshCollider with collide-and-slide,
    // walkable slope handling, step-up, and ground snapping.
    CapsuleMoveResult moveCapsule(const Capsule& capsule, const glm::vec3& delta,
                                  const MeshCollider& collider, const CapsuleMoveOptions& options = {});

    // ECS component for a capsule character in triangle-mesh worlds.
    // Transform::position represents the character's feet (bottom-most point of the capsule).
    struct CapsuleCharacterController {
        CharacterMode mode = CharacterMode::Walk;
        bool enabled = true;
        float radius = 0.35f;
        float height = 1.8f;
        float eyeHeight = 1.62f; // Above feet
        float walkSpeed = 4.3f;
        float sprintMultiplier = 1.5f;
        float flySpeed = 10.0f;
        float flySprintMultiplier = 2.5f;
        float jumpSpeed = 8.0f;
        float gravity = 28.0f;
        float maxFallSpeed = 60.0f;
        float maxSlopeAngle = 45.0f;      // Degrees
        float stepHeight = 0.5f;          // Walk mode and grounded only
        float groundSnapDistance = 0.35f; // Walk mode when grounded and not jumping
        float skin = 1e-3f;
        float acceleration = 40.0f; // Towards target velocity on XZ (ground); 1/4 in air
        float yaw = -90.0f;         // Degrees; -90 = looking along -Z
        float pitch = 0.0f;         // Degrees, clamped to +-89
        CharacterInput input;       // Written by the game each frame

        // --- State (written by stepCapsuleCharacter) ---
        glm::vec3 velocity{0.0f};
        glm::vec3 groundNormal{0.0f, 1.0f, 0.0f};
        bool grounded = false;
        bool onSteepSlope = false;
        bool stepped = false;
        bool snapped = false;
        glm::vec3 previousPosition{0.0f}; // Position at start of last fixed step (camera interpolation)

        void look(float deltaYawDegrees, float deltaPitchDegrees);
        glm::vec3 forward() const;
        glm::vec3 flatForward() const;
        glm::vec3 right() const;
        void setMode(CharacterMode newMode); // Resets velocity.y = 0 when switching to Fly
        void toggleMode();

        Capsule capsule(const glm::vec3& feet) const { return Capsule{feet, radius, height}; }
    };

    inline glm::vec3 capsuleCharacterEyePosition(const CapsuleCharacterController& controller,
                                                 const Transform& transform, float alpha) {
        return glm::mix(controller.previousPosition, transform.position, alpha) +
               glm::vec3(0.0f, controller.eyeHeight, 0.0f);
    }

    // One fixed step for a capsule character against a mesh world: pure, testable without ECS.
    glm::vec3 stepCapsuleCharacter(CapsuleCharacterController& controller, const glm::vec3& feet, float dt,
                                   const MeshCollider& collider);
    void stepCapsuleCharacter(CapsuleCharacterController& controller, Transform& transform, float dt,
                              const MeshCollider& collider);

    // Registers in SystemPhase::Fixed: steps every entity with <CapsuleCharacterController, Transform>.
    // `collider` must outlive the World.
    void addCapsuleCharacterSystem(World& world, const MeshCollider& collider);

    // Component for a camera entity that follows a CapsuleCharacterController at eye height.
    struct CapsuleCharacterCamera {
        Entity target;
    };

    // Registers in SystemPhase::Render: interpolates camera position with time.alpha() + eyeHeight
    // and aligns rotation with controller.forward(). `time` must outlive the World.
    void addCapsuleCharacterCameraSystem(World& world, const Time& time);

} // namespace mge
