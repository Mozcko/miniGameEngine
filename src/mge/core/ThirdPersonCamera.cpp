#include <mge/core/ThirdPersonCamera.h>

#include <algorithm>
#include <cmath>
#include <limits>

#include <glm/gtc/quaternion.hpp>

namespace mge {

    namespace {
        constexpr float kMaxAbsPitch = 89.0f;
        constexpr float kMinSafeDistance = 0.01f;
        constexpr float kEpsilon = 1e-5f;
        constexpr float kMaxVerticalDot = 0.9995f;

        // TODO(#229): use mge::damp once it lands
        float dampAlpha(float rate, float dt) {
            if (rate <= 0.0f) return 1.0f;
            return 1.0f - std::exp(-rate * dt);
        }

        float wrapAngle180(float degrees) {
            float wrapped = std::fmod(degrees + 180.0f, 360.0f);
            if (wrapped < 0.0f) wrapped += 360.0f;
            return wrapped - 180.0f;
        }

        float shortestAngleDelta(float fromDegrees, float toDegrees) {
            return wrapAngle180(toDegrees - fromDegrees);
        }

        glm::vec3 forwardFromAngles(float yawDegrees, float pitchDegrees) {
            const float yawRad = glm::radians(yawDegrees);
            const float pitchRad = glm::radians(pitchDegrees);
            const float cosPitch = std::cos(pitchRad);
            return glm::normalize(
                glm::vec3(std::cos(yawRad) * cosPitch, std::sin(pitchRad), std::sin(yawRad) * cosPitch));
        }

        glm::vec3 rightFromYaw(float yawDegrees) {
            const float yawRad = glm::radians(yawDegrees);
            return glm::vec3(-std::sin(yawRad), 0.0f, std::cos(yawRad));
        }

        glm::vec3 safeLookDirection(const glm::vec3& dir, const glm::vec3& fallback) {
            const float lenSq = glm::dot(dir, dir);
            if (lenSq < kEpsilon * kEpsilon) return fallback;
            const glm::vec3 normalized = dir / std::sqrt(lenSq);
            if (std::abs(normalized.y) > kMaxVerticalDot) return fallback;
            return normalized;
        }
    } // namespace

    ThirdPersonCamera::ThirdPersonCamera(const ThirdPersonCameraConfig& initialConfig, float yawDegrees,
                                         float pitchDegrees)
        : m_config(initialConfig) {
        m_yaw = wrapAngle180(yawDegrees);
        m_pitch = std::clamp(pitchDegrees, clampedMinPitch(), clampedMaxPitch());
        m_targetYaw = m_yaw;
        m_targetPitch = m_pitch;

        m_desiredDistance = std::clamp(m_config.distance, clampedMinDistance(), clampedMaxDistance());
        m_smoothedDistance = m_desiredDistance;
        m_currentDistance = m_desiredDistance;

        m_lookDirection = forward();
        m_position = computeIdealPosition();
    }

    float ThirdPersonCamera::clampedMinPitch() const {
        const float lo = std::min(m_config.minPitch, m_config.maxPitch);
        return std::clamp(lo, -kMaxAbsPitch, kMaxAbsPitch);
    }

    float ThirdPersonCamera::clampedMaxPitch() const {
        const float hi = std::max(m_config.minPitch, m_config.maxPitch);
        return std::clamp(hi, -kMaxAbsPitch, kMaxAbsPitch);
    }

    float ThirdPersonCamera::clampedMinDistance() const {
        return std::max(kMinSafeDistance, m_config.minDistance);
    }

    float ThirdPersonCamera::clampedMaxDistance() const {
        return std::max(clampedMinDistance(), m_config.maxDistance);
    }

    glm::vec3 ThirdPersonCamera::forward() const {
        return forwardFromAngles(m_yaw, m_pitch);
    }

    glm::vec3 ThirdPersonCamera::flatForward() const {
        const float yawRad = glm::radians(m_yaw);
        return glm::vec3(std::cos(yawRad), 0.0f, std::sin(yawRad));
    }

    glm::vec3 ThirdPersonCamera::right() const {
        return rightFromYaw(m_yaw);
    }

    glm::vec3 ThirdPersonCamera::computeIdealPosition() const {
        const glm::vec3 fwd = forward();
        const glm::vec3 r = right();
        const glm::vec3 up = glm::normalize(glm::cross(r, fwd));
        const glm::vec3 shoulderWorld =
            r * m_config.shoulderOffset.x + up * m_config.shoulderOffset.y + fwd * m_config.shoulderOffset.z;
        return m_pivot - fwd * m_smoothedDistance + shoulderWorld;
    }

    glm::vec3 ThirdPersonCamera::computeDesiredLookDirection() const {
        const glm::vec3 orbitFwd = forward();
        if (!m_lockOnTarget.has_value() || m_config.lockOnLookBias <= 0.0f) return orbitFwd;
        const float bias = std::clamp(m_config.lockOnLookBias, 0.0f, 1.0f);
        const glm::vec3 focus = glm::mix(m_pivot, *m_lockOnTarget, bias);
        return safeLookDirection(focus - m_position, orbitFwd);
    }

    float ThirdPersonCamera::resolveCollisionDistance(const glm::vec3& armDir, float idealDist) {
        m_isColliding = false;
        if (!m_collisionProbe) return idealDist;
        const auto hit = m_collisionProbe(m_pivot, armDir, idealDist);
        if (!hit.has_value()) return idealDist;

        const float clampedHit = std::clamp(*hit, 0.0f, idealDist);
        const float minFloor = std::max(0.0f, m_config.minCollisionDistance);
        const float floorDist = std::min(minFloor, clampedHit);
        const float radius = std::max(0.0f, m_config.collisionRadius);
        const float resolved = std::clamp(clampedHit - radius, floorDist, idealDist);
        m_isColliding = (resolved < idealDist - 1e-4f);
        return resolved;
    }

    void ThirdPersonCamera::setAngles(float yawDegrees, float pitchDegrees, bool snapImmediate) {
        m_targetYaw = wrapAngle180(yawDegrees);
        m_targetPitch = std::clamp(pitchDegrees, clampedMinPitch(), clampedMaxPitch());
        if (snapImmediate) {
            m_yaw = m_targetYaw;
            m_pitch = m_targetPitch;
            m_lookDirection = computeDesiredLookDirection();
        }
    }

    void ThirdPersonCamera::setDistance(float distance, bool snapImmediate) {
        m_desiredDistance = std::clamp(distance, clampedMinDistance(), clampedMaxDistance());
        if (snapImmediate) {
            m_smoothedDistance = m_desiredDistance;
            m_currentDistance = m_desiredDistance;
        }
    }

    void ThirdPersonCamera::setLockOnTarget(const glm::vec3& worldPosition) {
        m_lockOnTarget = worldPosition;
        m_hasLockOnLook = true;
    }

    void ThirdPersonCamera::clearLockOn() {
        m_lockOnTarget.reset();
    }

    void ThirdPersonCamera::snap(const glm::vec3& targetPosition) {
        m_pivot = targetPosition + m_config.pivotOffset;

        if (m_lockOnTarget.has_value()) {
            const glm::vec3 toTarget = *m_lockOnTarget - m_pivot;
            const float dist = glm::length(toTarget);
            if (m_config.lockOnMaxDistance > 0.0f && dist > m_config.lockOnMaxDistance) {
                m_lockOnTarget.reset();
            } else if (dist > kEpsilon) {
                const glm::vec3 dir = toTarget / dist;
                m_targetYaw = wrapAngle180(glm::degrees(std::atan2(dir.z, dir.x)));
                const float rawPitch = glm::degrees(std::asin(std::clamp(dir.y, -1.0f, 1.0f)));
                m_targetPitch =
                    std::clamp(rawPitch + m_config.lockOnPitchOffset, clampedMinPitch(), clampedMaxPitch());
            }
        }

        m_yaw = m_targetYaw;
        m_pitch = std::clamp(m_targetPitch, clampedMinPitch(), clampedMaxPitch());
        m_desiredDistance = std::clamp(m_desiredDistance, clampedMinDistance(), clampedMaxDistance());
        m_smoothedDistance = m_desiredDistance;

        const glm::vec3 idealPos = computeIdealPosition();
        const glm::vec3 toIdeal = idealPos - m_pivot;
        const float idealDist = glm::length(toIdeal);

        if (idealDist > kEpsilon) {
            const glm::vec3 armDir = toIdeal / idealDist;
            m_currentDistance = resolveCollisionDistance(armDir, idealDist);
            m_position = m_pivot + armDir * m_currentDistance;
        } else {
            m_isColliding = false;
            m_currentDistance = 0.0f;
            m_position = m_pivot;
        }

        m_lookDirection = computeDesiredLookDirection();
        m_hasLockOnLook = m_lockOnTarget.has_value();
        m_initialized = true;
    }

    void ThirdPersonCamera::update(float dt, const glm::vec3& targetPosition,
                                   const ThirdPersonCameraInput& input) {
        if (dt <= 0.0f) return;

        // 1. Follow target pivot (snap on first frame so the camera never flies in from the origin)
        const glm::vec3 desiredPivot = targetPosition + m_config.pivotOffset;
        if (!m_initialized) {
            m_pivot = desiredPivot;
        } else {
            m_pivot = glm::mix(m_pivot, desiredPivot, dampAlpha(m_config.followSmoothing, dt));
        }

        // 2. Zoom (allowed both in free orbit and while locked on)
        m_desiredDistance = std::clamp(m_desiredDistance - input.zoom * m_config.zoomSpeed,
                                       clampedMinDistance(), clampedMaxDistance());
        if (!m_initialized) {
            m_smoothedDistance = m_desiredDistance;
        } else {
            m_smoothedDistance =
                glm::mix(m_smoothedDistance, m_desiredDistance, dampAlpha(m_config.zoomSmoothing, dt));
        }

        // 3. Auto-break lock-on if target exceeds max distance
        if (m_lockOnTarget.has_value() && m_config.lockOnMaxDistance > 0.0f) {
            if (glm::distance(m_pivot, *m_lockOnTarget) > m_config.lockOnMaxDistance) {
                m_lockOnTarget.reset();
            }
        }

        // 4. Orbit orientation (lock-on overrides manual look input)
        const float minP = clampedMinPitch();
        const float maxP = clampedMaxPitch();
        if (m_lockOnTarget.has_value()) {
            const glm::vec3 toTarget = *m_lockOnTarget - m_pivot;
            const float dist = glm::length(toTarget);
            if (dist > kEpsilon) {
                const glm::vec3 dir = toTarget / dist;
                m_targetYaw = wrapAngle180(glm::degrees(std::atan2(dir.z, dir.x)));
                const float rawPitch = glm::degrees(std::asin(std::clamp(dir.y, -1.0f, 1.0f)));
                m_targetPitch = std::clamp(rawPitch + m_config.lockOnPitchOffset, minP, maxP);
            }
            const float lockAlpha = dampAlpha(m_config.lockOnSmoothing, dt);
            m_yaw = wrapAngle180(m_yaw + shortestAngleDelta(m_yaw, m_targetYaw) * lockAlpha);
            m_pitch = std::clamp(glm::mix(m_pitch, m_targetPitch, lockAlpha), minP, maxP);
        } else {
            const float ySign = m_config.invertY ? -1.0f : 1.0f;
            const float deltaPitch = input.lookY * m_config.sensitivity * ySign;
            m_targetYaw = wrapAngle180(m_targetYaw + input.lookX * m_config.sensitivity);
            m_targetPitch = std::clamp(m_targetPitch - deltaPitch, minP, maxP);
            const float rotAlpha = dampAlpha(m_config.rotationSmoothing, dt);
            m_yaw = wrapAngle180(m_yaw + shortestAngleDelta(m_yaw, m_targetYaw) * rotAlpha);
            m_pitch = std::clamp(glm::mix(m_pitch, m_targetPitch, rotAlpha), minP, maxP);
        }

        // 5. Spring-arm collision avoidance (fast pull-in, smooth recovery)
        const glm::vec3 idealPos = computeIdealPosition();
        const glm::vec3 toIdeal = idealPos - m_pivot;
        const float idealDist = glm::length(toIdeal);

        if (idealDist > kEpsilon) {
            const glm::vec3 armDir = toIdeal / idealDist;
            const float targetDist = resolveCollisionDistance(armDir, idealDist);

            if (!m_initialized) {
                m_currentDistance = targetDist;
            } else if (targetDist < m_currentDistance) {
                const float pullAlpha = dampAlpha(m_config.collisionPullInSpeed, dt);
                m_currentDistance = glm::mix(m_currentDistance, targetDist, pullAlpha);
            } else {
                const float recoverAlpha = dampAlpha(m_config.collisionRecoverSpeed, dt);
                m_currentDistance = glm::mix(m_currentDistance, targetDist, recoverAlpha);
            }

            m_position = m_pivot + armDir * m_currentDistance;
        } else {
            m_isColliding = false;
            m_currentDistance = 0.0f;
            m_position = m_pivot;
        }

        // 6. Look direction (smoothly blends focus framing on lock-on/unlock without snapping)
        const glm::vec3 desiredLook = computeDesiredLookDirection();
        if (!m_initialized) {
            m_lookDirection = desiredLook;
        } else if (m_lockOnTarget.has_value()) {
            const float lookAlpha = dampAlpha(m_config.lockOnSmoothing, dt);
            const glm::vec3 blended = glm::mix(m_lookDirection, desiredLook, lookAlpha);
            m_lookDirection = safeLookDirection(blended, desiredLook);
        } else if (m_hasLockOnLook) {
            const float lookAlpha = dampAlpha(m_config.rotationSmoothing, dt);
            const glm::vec3 blended = glm::mix(m_lookDirection, desiredLook, lookAlpha);
            m_lookDirection = safeLookDirection(blended, desiredLook);
            if (glm::dot(m_lookDirection, desiredLook) > 0.9999f) {
                m_lookDirection = desiredLook;
                m_hasLockOnLook = false;
            }
        } else {
            m_lookDirection = desiredLook;
        }

        m_initialized = true;
    }

    void ThirdPersonCamera::apply(Transform& transform) const {
        transform.position = m_position;
        const glm::vec3 safeDir = safeLookDirection(m_lookDirection, forward());
        // quatLookAt (right-handed): rotation mapping local -Z to safeDir
        transform.rotation = glm::quatLookAt(safeDir, glm::vec3(0.0f, 1.0f, 0.0f));
    }

    std::optional<std::size_t> selectLockOnTarget(const glm::vec3& origin, const glm::vec3& viewForward,
                                                  const std::vector<glm::vec3>& candidates, float maxDistance,
                                                  float maxAngleDegrees) {
        if (candidates.empty() || maxDistance <= 0.0f) return std::nullopt;
        const float fwdLenSq = glm::dot(viewForward, viewForward);
        if (fwdLenSq < kEpsilon * kEpsilon) return std::nullopt;
        const glm::vec3 fwd = viewForward / std::sqrt(fwdLenSq);
        const float minCos = std::cos(glm::radians(std::clamp(maxAngleDegrees, 0.0f, 180.0f)));

        std::optional<std::size_t> bestIndex;
        float bestScore = std::numeric_limits<float>::infinity();

        for (std::size_t i = 0; i < candidates.size(); ++i) {
            const glm::vec3 delta = candidates[i] - origin;
            const float dist = glm::length(delta);
            if (dist < kEpsilon || dist > maxDistance) continue;

            const float cosAngle = glm::dot(delta / dist, fwd);
            if (cosAngle < minCos) continue;

            // Lower score is better: prioritize alignment with view center, break ties by distance
            const float anglePenalty = 1.0f - cosAngle;
            const float distPenalty = (dist / maxDistance) * 0.25f;
            const float score = anglePenalty + distPenalty;
            if (score < bestScore) {
                bestScore = score;
                bestIndex = i;
            }
        }

        return bestIndex;
    }

    std::optional<std::size_t> switchLockOnTarget(const glm::vec3& origin, const glm::vec3& viewForward,
                                                  const glm::vec3& viewRight,
                                                  const std::vector<glm::vec3>& candidates,
                                                  std::size_t currentIndex, int direction,
                                                  float maxDistance) {
        if (direction == 0 || candidates.empty() || maxDistance <= 0.0f) return std::nullopt;
        const float fwdLenSq = glm::dot(viewForward, viewForward);
        const float rightLenSq = glm::dot(viewRight, viewRight);
        if (fwdLenSq < kEpsilon * kEpsilon || rightLenSq < kEpsilon * kEpsilon) return std::nullopt;

        const glm::vec3 fwd = viewForward / std::sqrt(fwdLenSq);
        const glm::vec3 r = viewRight / std::sqrt(rightLenSq);
        const float dirSign = (direction > 0) ? 1.0f : -1.0f;

        float currentLateral = 0.0f;
        if (currentIndex < candidates.size()) {
            const glm::vec3 currDelta = candidates[currentIndex] - origin;
            const float currDist = glm::length(currDelta);
            if (currDist > kEpsilon) {
                currentLateral = glm::dot(currDelta / currDist, r);
            }
        }

        std::optional<std::size_t> bestIndex;
        float bestMetric = std::numeric_limits<float>::infinity();

        for (std::size_t i = 0; i < candidates.size(); ++i) {
            if (i == currentIndex) continue;
            const glm::vec3 delta = candidates[i] - origin;
            const float dist = glm::length(delta);
            if (dist < kEpsilon || dist > maxDistance) continue;

            const glm::vec3 dir = delta / dist;
            if (glm::dot(dir, fwd) <= 0.0f) continue; // Ignore targets behind camera

            const float lateralDiff = (glm::dot(dir, r) - currentLateral) * dirSign;
            if (lateralDiff <= kEpsilon) continue; // Not on the requested side

            // Pick the closest target in angular lateral offset on the requested side
            if (lateralDiff < bestMetric) {
                bestMetric = lateralDiff;
                bestIndex = i;
            }
        }

        return bestIndex;
    }

} // namespace mge
