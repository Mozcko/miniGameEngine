#include <mge/core/Camera2D.h>

#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

namespace mge {

    namespace {
        glm::vec2 rotate2D(const glm::vec2& v, float radians) {
            if (radians == 0.0f) return v;
            const float c = std::cos(radians);
            const float s = std::sin(radians);
            return {v.x * c - v.y * s, v.x * s + v.y * c};
        }

        float signedUnitFloat(Pcg32& rng) {
            return rng.nextFloat01() * 2.0f - 1.0f;
        }
    }

    Camera2D::Camera2D(const glm::vec2& position, float zoom) : m_position(position) {
        setZoom(zoom);
    }

    float Camera2D::safeZoom() const {
        return std::max(m_zoom, kMinSafeZoom);
    }

    void Camera2D::setZoom(float zoom) {
        const float lo = std::max(minZoom, kMinSafeZoom);
        const float hi = std::max(maxZoom, lo);
        m_zoom = std::clamp(zoom, lo, hi);
    }

    void Camera2D::zoomBy(float factor) {
        if (factor <= 0.0f) return;
        setZoom(m_zoom * factor);
    }

    void Camera2D::zoomAt(float factor, const glm::vec2& screenPoint, const glm::vec2& viewport) {
        if (factor <= 0.0f) return;
        const glm::vec2 worldBefore = screenToWorld(screenPoint, viewport);
        setZoom(m_zoom * factor);
        const glm::vec2 worldAfter = screenToWorld(screenPoint, viewport);
        m_position += worldBefore - worldAfter;
    }

    glm::vec2 Camera2D::worldToScreen(const glm::vec2& worldPos, const glm::vec2& viewport) const {
        const glm::vec2 delta = worldPos - effectivePosition();
        const glm::vec2 rotated = rotate2D(delta, -effectiveRotation());
        return rotated * safeZoom() + viewport * 0.5f;
    }

    glm::vec2 Camera2D::screenToWorld(const glm::vec2& screenPos, const glm::vec2& viewport) const {
        const glm::vec2 centered = (screenPos - viewport * 0.5f) / safeZoom();
        const glm::vec2 unrotated = rotate2D(centered, effectiveRotation());
        return unrotated + effectivePosition();
    }

    glm::mat4 Camera2D::viewMatrix(const glm::vec2& viewport) const {
        const float z = safeZoom();
        const float angle = effectiveRotation();
        const glm::vec2 pos = effectivePosition();
        glm::mat4 view(1.0f);
        view = glm::translate(view, glm::vec3(viewport * 0.5f, 0.0f));
        view = glm::scale(view, glm::vec3(z, z, 1.0f));
        if (angle != 0.0f) view = glm::rotate(view, -angle, glm::vec3(0.0f, 0.0f, 1.0f));
        view = glm::translate(view, glm::vec3(-pos, 0.0f));
        return view;
    }

    glm::mat4 Camera2D::viewProjectionMatrix(const glm::vec2& viewport) const {
        if (viewport.x <= 0.0f || viewport.y <= 0.0f) return glm::mat4(1.0f);
        const glm::mat4 projection = glm::orthoRH_ZO(0.0f, viewport.x, viewport.y, 0.0f, -1.0f, 1.0f);
        return projection * viewMatrix(viewport);
    }

    Bounds2D Camera2D::visibleBounds(const glm::vec2& viewport) const {
        const glm::vec2 corners[4] = {
            screenToWorld({0.0f, 0.0f}, viewport),
            screenToWorld({viewport.x, 0.0f}, viewport),
            screenToWorld({viewport.x, viewport.y}, viewport),
            screenToWorld({0.0f, viewport.y}, viewport),
        };
        Bounds2D result{corners[0], corners[0]};
        for (int i = 1; i < 4; ++i) {
            result.min = glm::min(result.min, corners[i]);
            result.max = glm::max(result.max, corners[i]);
        }
        return result;
    }

    void Camera2D::clampToBounds(const glm::vec2& viewport) {
        if (!m_bounds) return;
        const Bounds2D& limit = *m_bounds;
        const glm::vec2 halfView =
            (viewport.x > 0.0f && viewport.y > 0.0f) ? (viewport * 0.5f) / safeZoom() : glm::vec2(0.0f);

        for (int axis = 0; axis < 2; ++axis) {
            const float minPos = limit.min[axis] + halfView[axis];
            const float maxPos = limit.max[axis] - halfView[axis];
            if (minPos > maxPos) {
                m_position[axis] = (limit.min[axis] + limit.max[axis]) * 0.5f;
            } else {
                m_position[axis] = std::clamp(m_position[axis], minPos, maxPos);
            }
        }
    }

    void Camera2D::follow(const glm::vec2& target, float dt) {
        if (dt <= 0.0f) return;

        const glm::vec2 halfDeadzone = glm::max(deadzoneHalfSize, glm::vec2(0.0f));
        glm::vec2 desired = m_position;
        for (int axis = 0; axis < 2; ++axis) {
            const float delta = target[axis] - m_position[axis];
            if (delta > halfDeadzone[axis]) {
                desired[axis] = target[axis] - halfDeadzone[axis];
            } else if (delta < -halfDeadzone[axis]) {
                desired[axis] = target[axis] + halfDeadzone[axis];
            }
        }

        if (followSmoothing <= 0.0f) {
            m_position = desired;
        } else {
            const float t = 1.0f - std::exp(-followSmoothing * dt);
            m_position = glm::mix(m_position, desired, t);
        }
    }

    void Camera2D::addTrauma(float amount) {
        if (amount <= 0.0f) return;
        m_trauma = std::clamp(m_trauma + amount, 0.0f, 1.0f);
    }

    void Camera2D::stopShake() {
        m_trauma = 0.0f;
        m_shakeOffset = glm::vec2(0.0f);
        m_shakeAngle = 0.0f;
    }

    void Camera2D::update(float dt) {
        if (dt <= 0.0f) return;

        if (m_trauma <= 0.0f) {
            m_shakeOffset = glm::vec2(0.0f);
            m_shakeAngle = 0.0f;
            return;
        }

        const float decay = std::max(shakeDecay, 0.0f);
        m_trauma = std::max(0.0f, m_trauma - decay * dt);
        if (m_trauma <= 0.0f) {
            m_shakeOffset = glm::vec2(0.0f);
            m_shakeAngle = 0.0f;
            return;
        }

        const float intensity = m_trauma * m_trauma;
        m_shakeOffset = {signedUnitFloat(m_rng) * maxShakeOffset.x * intensity,
                         signedUnitFloat(m_rng) * maxShakeOffset.y * intensity};
        m_shakeAngle = signedUnitFloat(m_rng) * maxShakeAngle * intensity;
    }
}
