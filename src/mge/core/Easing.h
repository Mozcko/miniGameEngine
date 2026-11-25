#pragma once

#include <cmath>

namespace mge {

    // Standard easing functions mapping normalized time t in [0, 1] to an eased progress value.
    // Most curves stay within [0, 1]; Back and Elastic intentionally overshoot.
    // All curves guarantee f(0) == 0 and f(1) == 1.
    enum class EaseType {
        Linear,
        InQuad,
        OutQuad,
        InOutQuad,
        InCubic,
        OutCubic,
        InOutCubic,
        InQuart,
        OutQuart,
        InOutQuart,
        InQuint,
        OutQuint,
        InOutQuint,
        InSine,
        OutSine,
        InOutSine,
        InExpo,
        OutExpo,
        InOutExpo,
        InCirc,
        OutCirc,
        InOutCirc,
        InBack,
        OutBack,
        InOutBack,
        InElastic,
        OutElastic,
        InOutElastic,
        InBounce,
        OutBounce,
        InOutBounce,
        SmoothStep,
        SmootherStep
    };

    namespace ease {

        using EaseFunc = float (*)(float);

        inline float clamp01(float t) {
            if (t <= 0.0f) return 0.0f;
            if (t >= 1.0f) return 1.0f;
            return t;
        }

        inline float linear(float t) {
            return clamp01(t);
        }

        // --- Quadratic ---
        inline float inQuad(float t) {
            t = clamp01(t);
            return t * t;
        }

        inline float outQuad(float t) {
            t = clamp01(t);
            const float inv = 1.0f - t;
            return 1.0f - inv * inv;
        }

        inline float inOutQuad(float t) {
            t = clamp01(t);
            if (t < 0.5f) return 2.0f * t * t;
            const float v = -2.0f * t + 2.0f;
            return 1.0f - (v * v) * 0.5f;
        }

        // --- Cubic ---
        inline float inCubic(float t) {
            t = clamp01(t);
            return t * t * t;
        }

        inline float outCubic(float t) {
            t = clamp01(t);
            const float inv = 1.0f - t;
            return 1.0f - inv * inv * inv;
        }

        inline float inOutCubic(float t) {
            t = clamp01(t);
            if (t < 0.5f) return 4.0f * t * t * t;
            const float v = -2.0f * t + 2.0f;
            return 1.0f - (v * v * v) * 0.5f;
        }

        // --- Quartic ---
        inline float inQuart(float t) {
            t = clamp01(t);
            const float t2 = t * t;
            return t2 * t2;
        }

        inline float outQuart(float t) {
            t = clamp01(t);
            const float inv = 1.0f - t;
            const float inv2 = inv * inv;
            return 1.0f - inv2 * inv2;
        }

        inline float inOutQuart(float t) {
            t = clamp01(t);
            if (t < 0.5f) {
                const float t2 = t * t;
                return 8.0f * t2 * t2;
            }
            const float v = -2.0f * t + 2.0f;
            const float v2 = v * v;
            return 1.0f - (v2 * v2) * 0.5f;
        }

        // --- Quintic ---
        inline float inQuint(float t) {
            t = clamp01(t);
            const float t2 = t * t;
            return t2 * t2 * t;
        }

        inline float outQuint(float t) {
            t = clamp01(t);
            const float inv = 1.0f - t;
            const float inv2 = inv * inv;
            return 1.0f - inv2 * inv2 * inv;
        }

        inline float inOutQuint(float t) {
            t = clamp01(t);
            if (t < 0.5f) {
                const float t2 = t * t;
                return 16.0f * t2 * t2 * t;
            }
            const float v = -2.0f * t + 2.0f;
            const float v2 = v * v;
            return 1.0f - (v2 * v2 * v) * 0.5f;
        }

        // --- Sine ---
        inline float inSine(float t) {
            constexpr float kHalfPi = 1.57079632679489661923f;
            t = clamp01(t);
            if (t == 0.0f) return 0.0f;
            if (t == 1.0f) return 1.0f;
            return 1.0f - std::cos(t * kHalfPi);
        }

        inline float outSine(float t) {
            constexpr float kHalfPi = 1.57079632679489661923f;
            t = clamp01(t);
            if (t == 0.0f) return 0.0f;
            if (t == 1.0f) return 1.0f;
            return std::sin(t * kHalfPi);
        }

        inline float inOutSine(float t) {
            constexpr float kPi = 3.14159265358979323846f;
            t = clamp01(t);
            if (t == 0.0f) return 0.0f;
            if (t == 1.0f) return 1.0f;
            return -(std::cos(kPi * t) - 1.0f) * 0.5f;
        }

        // --- Exponential ---
        inline float inExpo(float t) {
            if (t <= 0.0f) return 0.0f;
            if (t >= 1.0f) return 1.0f;
            return std::pow(2.0f, 10.0f * t - 10.0f);
        }

        inline float outExpo(float t) {
            if (t <= 0.0f) return 0.0f;
            if (t >= 1.0f) return 1.0f;
            return 1.0f - std::pow(2.0f, -10.0f * t);
        }

        inline float inOutExpo(float t) {
            if (t <= 0.0f) return 0.0f;
            if (t >= 1.0f) return 1.0f;
            if (t < 0.5f) return std::pow(2.0f, 20.0f * t - 10.0f) * 0.5f;
            return (2.0f - std::pow(2.0f, -20.0f * t + 10.0f)) * 0.5f;
        }

        // --- Circular ---
        inline float inCirc(float t) {
            if (t <= 0.0f) return 0.0f;
            if (t >= 1.0f) return 1.0f;
            return 1.0f - std::sqrt(1.0f - t * t);
        }

        inline float outCirc(float t) {
            if (t <= 0.0f) return 0.0f;
            if (t >= 1.0f) return 1.0f;
            const float v = t - 1.0f;
            return std::sqrt(1.0f - v * v);
        }

        inline float inOutCirc(float t) {
            if (t <= 0.0f) return 0.0f;
            if (t >= 1.0f) return 1.0f;
            if (t < 0.5f) {
                const float v = 2.0f * t;
                return (1.0f - std::sqrt(1.0f - v * v)) * 0.5f;
            }
            const float v = -2.0f * t + 2.0f;
            return (std::sqrt(1.0f - v * v) + 1.0f) * 0.5f;
        }

        // --- Back (overshoot) ---
        inline float inBack(float t) {
            constexpr float kC1 = 1.70158f;
            constexpr float kC3 = kC1 + 1.0f;
            if (t <= 0.0f) return 0.0f;
            if (t >= 1.0f) return 1.0f;
            return kC3 * t * t * t - kC1 * t * t;
        }

        inline float outBack(float t) {
            constexpr float kC1 = 1.70158f;
            constexpr float kC3 = kC1 + 1.0f;
            if (t <= 0.0f) return 0.0f;
            if (t >= 1.0f) return 1.0f;
            const float v = t - 1.0f;
            return 1.0f + kC3 * v * v * v + kC1 * v * v;
        }

        inline float inOutBack(float t) {
            constexpr float kC1 = 1.70158f;
            constexpr float kC2 = kC1 * 1.525f;
            if (t <= 0.0f) return 0.0f;
            if (t >= 1.0f) return 1.0f;
            if (t < 0.5f) {
                const float v = 2.0f * t;
                return (v * v * ((kC2 + 1.0f) * v - kC2)) * 0.5f;
            }
            const float v = 2.0f * t - 2.0f;
            return (v * v * ((kC2 + 1.0f) * v + kC2) + 2.0f) * 0.5f;
        }

        // --- Elastic ---
        inline float inElastic(float t) {
            constexpr float kC4 = (2.0f * 3.14159265358979323846f) / 3.0f;
            if (t <= 0.0f) return 0.0f;
            if (t >= 1.0f) return 1.0f;
            return -std::pow(2.0f, 10.0f * t - 10.0f) * std::sin((t * 10.0f - 10.75f) * kC4);
        }

        inline float outElastic(float t) {
            constexpr float kC4 = (2.0f * 3.14159265358979323846f) / 3.0f;
            if (t <= 0.0f) return 0.0f;
            if (t >= 1.0f) return 1.0f;
            return std::pow(2.0f, -10.0f * t) * std::sin((t * 10.0f - 0.75f) * kC4) + 1.0f;
        }

        inline float inOutElastic(float t) {
            constexpr float kC5 = (2.0f * 3.14159265358979323846f) / 4.5f;
            if (t <= 0.0f) return 0.0f;
            if (t >= 1.0f) return 1.0f;
            if (t < 0.5f) {
                return -(std::pow(2.0f, 20.0f * t - 10.0f) *
                         std::sin((20.0f * t - 11.125f) * kC5)) *
                       0.5f;
            }
            return (std::pow(2.0f, -20.0f * t + 10.0f) *
                    std::sin((20.0f * t - 11.125f) * kC5)) *
                       0.5f +
                   1.0f;
        }

        // --- Bounce ---
        inline float outBounce(float t) {
            constexpr float kN1 = 7.5625f;
            constexpr float kD1 = 2.75f;
            t = clamp01(t);
            if (t < 1.0f / kD1) {
                return kN1 * t * t;
            }
            if (t < 2.0f / kD1) {
                t -= 1.5f / kD1;
                return kN1 * t * t + 0.75f;
            }
            if (t < 2.5f / kD1) {
                t -= 2.25f / kD1;
                return kN1 * t * t + 0.9375f;
            }
            t -= 2.625f / kD1;
            return kN1 * t * t + 0.984375f;
        }

        inline float inBounce(float t) {
            return 1.0f - outBounce(1.0f - clamp01(t));
        }

        inline float inOutBounce(float t) {
            t = clamp01(t);
            if (t < 0.5f) return (1.0f - outBounce(1.0f - 2.0f * t)) * 0.5f;
            return (1.0f + outBounce(2.0f * t - 1.0f)) * 0.5f;
        }

        // --- Smoothstep ---
        inline float smoothStep(float t) {
            t = clamp01(t);
            return t * t * (3.0f - 2.0f * t);
        }

        inline float smootherStep(float t) {
            t = clamp01(t);
            return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
        }

        // Maps an EaseType enum value to its corresponding easing function pointer.
        inline EaseFunc fromType(EaseType type) {
            switch (type) {
                case EaseType::Linear: return linear;
                case EaseType::InQuad: return inQuad;
                case EaseType::OutQuad: return outQuad;
                case EaseType::InOutQuad: return inOutQuad;
                case EaseType::InCubic: return inCubic;
                case EaseType::OutCubic: return outCubic;
                case EaseType::InOutCubic: return inOutCubic;
                case EaseType::InQuart: return inQuart;
                case EaseType::OutQuart: return outQuart;
                case EaseType::InOutQuart: return inOutQuart;
                case EaseType::InQuint: return inQuint;
                case EaseType::OutQuint: return outQuint;
                case EaseType::InOutQuint: return inOutQuint;
                case EaseType::InSine: return inSine;
                case EaseType::OutSine: return outSine;
                case EaseType::InOutSine: return inOutSine;
                case EaseType::InExpo: return inExpo;
                case EaseType::OutExpo: return outExpo;
                case EaseType::InOutExpo: return inOutExpo;
                case EaseType::InCirc: return inCirc;
                case EaseType::OutCirc: return outCirc;
                case EaseType::InOutCirc: return inOutCirc;
                case EaseType::InBack: return inBack;
                case EaseType::OutBack: return outBack;
                case EaseType::InOutBack: return inOutBack;
                case EaseType::InElastic: return inElastic;
                case EaseType::OutElastic: return outElastic;
                case EaseType::InOutElastic: return inOutElastic;
                case EaseType::InBounce: return inBounce;
                case EaseType::OutBounce: return outBounce;
                case EaseType::InOutBounce: return inOutBounce;
                case EaseType::SmoothStep: return smoothStep;
                case EaseType::SmootherStep: return smootherStep;
            }
            return linear;
        }

        inline float evaluate(EaseType type, float t) {
            return fromType(type)(t);
        }
    }
}
