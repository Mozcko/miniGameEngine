#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace mge {

    // Frame-rate independent interpolation factor in [0, 1] for exponential decay: 1 - exp(-lambda * dt).
    // Returns 0 when dt <= 0 or lambda <= 0.
    inline float dampFactor(float lambda, float dt) {
        if (dt <= 0.0f || lambda <= 0.0f) return 0.0f;
        return -std::expm1(-lambda * dt);
    }

    // Shortest signed angular difference from current to target, in radians in (-pi, pi].
    inline float deltaAngle(float current, float target) {
        constexpr float kPi = 3.14159265358979323846f;
        constexpr float kTwoPi = 2.0f * kPi;
        float delta = std::fmod(target - current, kTwoPi);
        if (delta > kPi) delta -= kTwoPi;
        if (delta <= -kPi) delta += kTwoPi;
        return delta;
    }

    // --- Exponential decay smoothing (first-order, frame-rate independent) ---
    // Approaches target asymptotically; higher lambda = faster convergence.
    // Returns current unchanged when dt <= 0 or lambda <= 0.

    inline float damp(float current, float target, float lambda, float dt) {
        return glm::mix(current, target, dampFactor(lambda, dt));
    }

    inline glm::vec2 damp(const glm::vec2& current, const glm::vec2& target, float lambda, float dt) {
        return glm::mix(current, target, dampFactor(lambda, dt));
    }

    inline glm::vec3 damp(const glm::vec3& current, const glm::vec3& target, float lambda, float dt) {
        return glm::mix(current, target, dampFactor(lambda, dt));
    }

    inline glm::vec4 damp(const glm::vec4& current, const glm::vec4& target, float lambda, float dt) {
        return glm::mix(current, target, dampFactor(lambda, dt));
    }

    inline glm::quat damp(const glm::quat& current, const glm::quat& target, float lambda, float dt) {
        const float factor = dampFactor(lambda, dt);
        if (factor <= 0.0f) return current;
        return glm::normalize(glm::slerp(glm::normalize(current), glm::normalize(target), factor));
    }

    // Exponential decay smoothing for angles (in radians) along the shortest arc.
    inline float dampAngle(float current, float target, float lambda, float dt) {
        return current + deltaAngle(current, target) * dampFactor(lambda, dt);
    }

    // --- Critically damped spring smoothing (second-order, frame-rate independent) ---
    // Tracks velocity for smooth acceleration and approaches target without overshooting.
    // smoothTime is the approximate time (seconds) to reach the target; smaller = faster.
    // Returns current unchanged (without modifying velocity) when dt <= 0.

    inline float smoothDamp(float current, float target, float& velocity, float smoothTime, float dt,
                            float maxSpeed = std::numeric_limits<float>::infinity()) {
        if (dt <= 0.0f) return current;

        constexpr float kMinSmoothTime = 1e-4f;
        smoothTime = std::max(kMinSmoothTime, smoothTime);
        const float omega = 2.0f / smoothTime;
        const float expFactor = std::exp(-omega * dt);

        float change = current - target;
        if (std::isfinite(maxSpeed)) {
            const float maxChange = std::max(0.0f, maxSpeed) * smoothTime;
            change = std::clamp(change, -maxChange, maxChange);
        }

        const float clampedTarget = current - change;
        const float temp = (velocity + omega * change) * dt;
        velocity = (velocity - omega * temp) * expFactor;
        float output = clampedTarget + (change + temp) * expFactor;

        // Prevent overshooting the original target
        if ((target - current) * (output - target) > 0.0f) {
            output = target;
            velocity = 0.0f;
        }
        return output;
    }

    namespace detail {
        template <typename Vec>
        inline Vec smoothDampVec(const Vec& current, const Vec& target, Vec& velocity, float smoothTime,
                                 float dt, float maxSpeed) {
            if (dt <= 0.0f) return current;

            constexpr float kMinSmoothTime = 1e-4f;
            smoothTime = std::max(kMinSmoothTime, smoothTime);
            const float omega = 2.0f / smoothTime;
            const float expFactor = std::exp(-omega * dt);

            Vec change = current - target;
            if (std::isfinite(maxSpeed)) {
                const float maxChange = std::max(0.0f, maxSpeed) * smoothTime;
                const float sqrDist = glm::dot(change, change);
                const float maxChangeSq = maxChange * maxChange;
                if (sqrDist > maxChangeSq) change *= maxChange / std::sqrt(sqrDist);
            }

            const Vec clampedTarget = current - change;
            const Vec temp = (velocity + omega * change) * dt;
            velocity = (velocity - omega * temp) * expFactor;
            Vec output = clampedTarget + (change + temp) * expFactor;

            // Prevent overshooting the original target
            if (glm::dot(target - current, output - target) > 0.0f) {
                output = target;
                velocity = Vec(0.0f);
            }
            return output;
        }
    }

    inline glm::vec2 smoothDamp(const glm::vec2& current, const glm::vec2& target, glm::vec2& velocity,
                                float smoothTime, float dt,
                                float maxSpeed = std::numeric_limits<float>::infinity()) {
        return detail::smoothDampVec(current, target, velocity, smoothTime, dt, maxSpeed);
    }

    inline glm::vec3 smoothDamp(const glm::vec3& current, const glm::vec3& target, glm::vec3& velocity,
                                float smoothTime, float dt,
                                float maxSpeed = std::numeric_limits<float>::infinity()) {
        return detail::smoothDampVec(current, target, velocity, smoothTime, dt, maxSpeed);
    }

    inline glm::vec4 smoothDamp(const glm::vec4& current, const glm::vec4& target, glm::vec4& velocity,
                                float smoothTime, float dt,
                                float maxSpeed = std::numeric_limits<float>::infinity()) {
        return detail::smoothDampVec(current, target, velocity, smoothTime, dt, maxSpeed);
    }

    // Critically damped spring smoothing for angles (in radians) along the shortest arc.
    inline float smoothDampAngle(float current, float target, float& velocity, float smoothTime, float dt,
                                 float maxSpeed = std::numeric_limits<float>::infinity()) {
        return smoothDamp(current, current + deltaAngle(current, target), velocity, smoothTime, dt, maxSpeed);
    }
}
