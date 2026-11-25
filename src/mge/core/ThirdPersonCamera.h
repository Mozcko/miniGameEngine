#pragma once

#include <mge/core/ecs/Transform.h>

#include <cstddef>
#include <functional>
#include <optional>
#include <vector>

#include <glm/glm.hpp>

namespace mge {

    // Input for ThirdPersonCamera, already translated from game actions/axes (mouse, gamepad, wheel).
    // Controllers never read keys directly so they stay platform-independent.
    struct ThirdPersonCameraInput {
        float lookX = 0.0f; // Horizontal orbit delta (right +, e.g. mouse pixels)
        float lookY = 0.0f; // Vertical orbit delta (screen-space down +, same as CameraInput)
        float zoom = 0.0f;  // Zoom delta (+ zooms in / reduces distance, - zooms out)
    };

    // Configuration for ThirdPersonCamera. Smoothing rates are exponential decay rates (1/s):
    // larger values converge faster; values <= 0 snap immediately.
    struct ThirdPersonCameraConfig {
        // --- Pivot & framing ---
        glm::vec3 pivotOffset{0.0f, 1.5f, 0.0f};    // World-space offset from target (e.g. chest/head)
        glm::vec3 shoulderOffset{0.0f, 0.0f, 0.0f}; // Camera-basis offset (x = right, y = up, z = forward)

        // --- Orbit & zoom ---
        float distance = 5.0f;     // Default boom arm length
        float minDistance = 1.5f;  // Minimum zoom distance
        float maxDistance = 12.0f; // Maximum zoom distance
        float minPitch = -75.0f;   // Degrees, clamped inside [-89, 89] (negative = looking down from above)
        float maxPitch = 65.0f;    // Degrees, clamped inside [-89, 89] (positive = looking up from below)
        float sensitivity = 0.15f; // Degrees per look input unit
        float zoomSpeed = 0.5f;    // Distance units per zoom input unit
        bool invertY = false;

        // --- Smoothing (frame-rate independent; <= 0 = instant) ---
        float followSmoothing = 12.0f;   // Pivot follow rate (1/s)
        float rotationSmoothing = 18.0f; // Orbit yaw/pitch rate (1/s)
        float zoomSmoothing = 10.0f;     // Distance zoom rate (1/s)

        // --- Collision (spring arm / wall avoidance) ---
        float collisionRadius = 0.25f;      // Pull-back margin from hit surface to avoid near-plane clip
        float minCollisionDistance = 0.5f;  // Floor distance when pulling in (never exceeds hit distance)
        float collisionPullInSpeed = 0.0f;  // Pull-in rate (1/s); <= 0 = instant to prevent clipping
        float collisionRecoverSpeed = 8.0f; // Extension recovery rate (1/s) once unobstructed

        // --- Target lock-on ---
        float lockOnSmoothing = 10.0f;    // Tracking rate while locked on (1/s)
        float lockOnPitchOffset = -10.0f; // Downward pitch bias (degrees) to frame both player and target
        float lockOnMaxDistance = 30.0f;  // Auto-break distance from pivot (<= 0 = unlimited)
        float lockOnLookBias = 0.35f;     // Focus blend [0..1] from pivot toward lock-on target
    };

    // Third-person orbit camera rig: smooth target following, yaw/pitch orbit and zoom, spring-arm
    // collision avoidance via a user-supplied ray probe, and target lock-on. Writes a Transform
    // (position + rotation) for the Camera component.
    class ThirdPersonCamera {
    public:
        // Probes along the segment [origin, origin + direction * maxDistance].
        // `direction` is unit length and `maxDistance > 0`. Returns hit distance t in [0, maxDistance]
        // if obstructed, or std::nullopt if clear.
        using CollisionProbe = std::function<std::optional<float>(
            const glm::vec3& origin, const glm::vec3& direction, float maxDistance)>;

        explicit ThirdPersonCamera(const ThirdPersonCameraConfig& initialConfig = {},
                                   float yawDegrees = -90.0f, float pitchDegrees = -15.0f);

        // Advances the camera by `dt` seconds following `targetPosition`.
        // dt <= 0 (paused) neither moves nor rotates.
        void update(float dt, const glm::vec3& targetPosition, const ThirdPersonCameraInput& input = {});

        // Immediately positions the camera at `targetPosition` with no smoothing lag (spawn/teleport).
        void snap(const glm::vec3& targetPosition);

        // Writes position and rotation (local -Z facing lookDirection()) into `transform`.
        void apply(Transform& transform) const;

        // --- Configuration & callbacks ---
        ThirdPersonCameraConfig& config() { return m_config; }
        const ThirdPersonCameraConfig& config() const { return m_config; }
        void setCollisionProbe(CollisionProbe probe) { m_collisionProbe = std::move(probe); }

        // --- Direct controls ---
        void setAngles(float yawDegrees, float pitchDegrees, bool snapImmediate = true);
        void setDistance(float distance, bool snapImmediate = true);

        // --- Target lock-on ---
        void setLockOnTarget(const glm::vec3& worldPosition);
        void clearLockOn();
        bool isLockedOn() const { return m_lockOnTarget.has_value(); }
        const std::optional<glm::vec3>& lockOnTarget() const { return m_lockOnTarget; }

        // --- State queries ---
        const glm::vec3& position() const { return m_position; }
        const glm::vec3& pivot() const { return m_pivot; }
        const glm::vec3& lookDirection() const { return m_lookDirection; }
        float yaw() const { return m_yaw; }     // Degrees in [-180, 180); -90 = looking toward -Z
        float pitch() const { return m_pitch; } // Degrees, clamped to [minPitch, maxPitch]
        float desiredDistance() const { return m_desiredDistance; }
        float currentDistance() const { return m_currentDistance; }
        bool isColliding() const { return m_isColliding; }

        glm::vec3 forward() const;     // View direction from current yaw/pitch
        glm::vec3 flatForward() const; // Horizontal forward on XZ plane (useful for character movement)
        glm::vec3 right() const;       // Horizontal right on XZ plane

    private:
        float clampedMinPitch() const;
        float clampedMaxPitch() const;
        float clampedMinDistance() const;
        float clampedMaxDistance() const;
        glm::vec3 computeIdealPosition() const;
        glm::vec3 computeDesiredLookDirection() const;
        float resolveCollisionDistance(const glm::vec3& armDir, float idealDist);

        ThirdPersonCameraConfig m_config;
        CollisionProbe m_collisionProbe;

        glm::vec3 m_pivot{0.0f};
        glm::vec3 m_position{0.0f};
        glm::vec3 m_lookDirection{0.0f, 0.0f, -1.0f};

        float m_yaw = -90.0f;
        float m_pitch = -15.0f;
        float m_targetYaw = -90.0f;
        float m_targetPitch = -15.0f;

        float m_desiredDistance = 5.0f;
        float m_smoothedDistance = 5.0f;
        float m_currentDistance = 5.0f;

        std::optional<glm::vec3> m_lockOnTarget;
        bool m_hasLockOnLook = false;
        bool m_isColliding = false;
        bool m_initialized = false;
    };

    // Selects the best lock-on candidate index within `maxDistance` of `origin` and within
    // `maxAngleDegrees` (half-angle cone) of `viewForward`, preferring targets closest to the
    // view center (with distance as a gentle tie-breaker). Returns std::nullopt if none qualify.
    std::optional<std::size_t> selectLockOnTarget(const glm::vec3& origin, const glm::vec3& viewForward,
                                                  const std::vector<glm::vec3>& candidates,
                                                  float maxDistance = 30.0f, float maxAngleDegrees = 60.0f);

    // Switches lock-on from `currentIndex` to the nearest valid candidate to the left (direction < 0)
    // or right (direction > 0) along `viewRight`, within `maxDistance` of `origin` and in front of
    // `viewForward`. Returns std::nullopt if direction == 0 or no candidate exists on that side.
    std::optional<std::size_t> switchLockOnTarget(const glm::vec3& origin, const glm::vec3& viewForward,
                                                  const glm::vec3& viewRight,
                                                  const std::vector<glm::vec3>& candidates,
                                                  std::size_t currentIndex, int direction,
                                                  float maxDistance = 30.0f);

} // namespace mge
