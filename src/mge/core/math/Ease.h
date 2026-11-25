#pragma once

#include <cmath>

namespace mge::ease {

    // Function pointer type for an easing curve: maps normalized progress t in [0, 1] to an eased value.
    // All curves satisfy f(0) == 0 and f(1) == 1. Overshooting curves (Back, Elastic) may leave [0, 1].
    using EaseFunc = float (*)(float);

    // Runtime selector for all built-in easing curves.
    enum class EaseType {
        Linear,
        InSine,
        OutSine,
        InOutSine,
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
        Smoothstep,
        Smootherstep
    };

    namespace detail {
        constexpr float kPi = 3.14159265358979323846f;
        constexpr float kBackC1 = 1.70158f;
        constexpr float kBackC2 = kBackC1 * 1.525f;
        constexpr float kBackC3 = kBackC1 + 1.0f;
        constexpr float kElasticC4 = (2.0f * kPi) / 3.0f;
        constexpr float kElasticC5 = (2.0f * kPi) / 4.5f;
        constexpr float kBounceN1 = 7.5625f;
        constexpr float kBounceD1 = 2.75f;

        constexpr float clamp01(float t) {
            return t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
        }
    }

    // --- Linear ---

    constexpr float linear(float t) {
        return t;
    }

    // --- Sine ---

    inline float inSine(float t) {
        return 1.0f - std::cos((t * detail::kPi) * 0.5f);
    }

    inline float outSine(float t) {
        return std::sin((t * detail::kPi) * 0.5f);
    }

    inline float inOutSine(float t) {
        return 0.5f * (1.0f - std::cos(detail::kPi * t));
    }

    // --- Quadratic ---

    constexpr float inQuad(float t) {
        return t * t;
    }

    constexpr float outQuad(float t) {
        float u = 1.0f - t;
        return 1.0f - u * u;
    }

    constexpr float inOutQuad(float t) {
        if (t < 0.5f) return 2.0f * t * t;
        float u = -2.0f * t + 2.0f;
        return 1.0f - (u * u) * 0.5f;
    }

    // --- Cubic ---

    constexpr float inCubic(float t) {
        return t * t * t;
    }

    constexpr float outCubic(float t) {
        float u = 1.0f - t;
        return 1.0f - u * u * u;
    }

    constexpr float inOutCubic(float t) {
        if (t < 0.5f) return 4.0f * t * t * t;
        float u = -2.0f * t + 2.0f;
        return 1.0f - (u * u * u) * 0.5f;
    }

    // --- Quartic ---

    constexpr float inQuart(float t) {
        float t2 = t * t;
        return t2 * t2;
    }

    constexpr float outQuart(float t) {
        float u = 1.0f - t;
        float u2 = u * u;
        return 1.0f - u2 * u2;
    }

    constexpr float inOutQuart(float t) {
        if (t < 0.5f) {
            float t2 = t * t;
            return 8.0f * t2 * t2;
        }
        float u = -2.0f * t + 2.0f;
        float u2 = u * u;
        return 1.0f - (u2 * u2) * 0.5f;
    }

    // --- Quintic ---

    constexpr float inQuint(float t) {
        float t2 = t * t;
        return t2 * t2 * t;
    }

    constexpr float outQuint(float t) {
        float u = 1.0f - t;
        float u2 = u * u;
        return 1.0f - u2 * u2 * u;
    }

    constexpr float inOutQuint(float t) {
        if (t < 0.5f) {
            float t2 = t * t;
            return 16.0f * t2 * t2 * t;
        }
        float u = -2.0f * t + 2.0f;
        float u2 = u * u;
        return 1.0f - (u2 * u2 * u) * 0.5f;
    }

    // --- Exponential ---

    inline float inExpo(float t) {
        if (t <= 0.0f) return 0.0f;
        return std::exp2(10.0f * t - 10.0f);
    }

    inline float outExpo(float t) {
        if (t >= 1.0f) return 1.0f;
        return 1.0f - std::exp2(-10.0f * t);
    }

    inline float inOutExpo(float t) {
        if (t <= 0.0f) return 0.0f;
        if (t >= 1.0f) return 1.0f;
        if (t < 0.5f) return std::exp2(20.0f * t - 10.0f) * 0.5f;
        return (2.0f - std::exp2(-20.0f * t + 10.0f)) * 0.5f;
    }

    // --- Circular ---

    inline float inCirc(float t) {
        float clamped = detail::clamp01(t);
        return 1.0f - std::sqrt(1.0f - clamped * clamped);
    }

    inline float outCirc(float t) {
        float u = detail::clamp01(t) - 1.0f;
        return std::sqrt(1.0f - u * u);
    }

    inline float inOutCirc(float t) {
        float clamped = detail::clamp01(t);
        if (clamped < 0.5f) {
            float u = 2.0f * clamped;
            return (1.0f - std::sqrt(1.0f - u * u)) * 0.5f;
        }
        float u = -2.0f * clamped + 2.0f;
        return (std::sqrt(1.0f - u * u) + 1.0f) * 0.5f;
    }

    // --- Back (overshoot) ---

    constexpr float inBack(float t) {
        return detail::kBackC3 * t * t * t - detail::kBackC1 * t * t;
    }

    constexpr float outBack(float t) {
        float u = t - 1.0f;
        return 1.0f + detail::kBackC3 * u * u * u + detail::kBackC1 * u * u;
    }

    constexpr float inOutBack(float t) {
        if (t < 0.5f) {
            float u = 2.0f * t;
            return (u * u * ((detail::kBackC2 + 1.0f) * u - detail::kBackC2)) * 0.5f;
        }
        float u = 2.0f * t - 2.0f;
        return (u * u * ((detail::kBackC2 + 1.0f) * u + detail::kBackC2) + 2.0f) * 0.5f;
    }

    // --- Elastic (oscillating overshoot) ---

    inline float inElastic(float t) {
        if (t <= 0.0f) return 0.0f;
        if (t >= 1.0f) return 1.0f;
        return -std::exp2(10.0f * t - 10.0f) * std::sin((t * 10.0f - 10.75f) * detail::kElasticC4);
    }

    inline float outElastic(float t) {
        if (t <= 0.0f) return 0.0f;
        if (t >= 1.0f) return 1.0f;
        return std::exp2(-10.0f * t) * std::sin((t * 10.0f - 0.75f) * detail::kElasticC4) + 1.0f;
    }

    inline float inOutElastic(float t) {
        if (t <= 0.0f) return 0.0f;
        if (t >= 1.0f) return 1.0f;
        float wave = std::sin((20.0f * t - 11.125f) * detail::kElasticC5);
        if (t < 0.5f) return -(std::exp2(20.0f * t - 10.0f) * wave) * 0.5f;
        return (std::exp2(-20.0f * t + 10.0f) * wave) * 0.5f + 1.0f;
    }

    // --- Bounce ---

    constexpr float outBounce(float t) {
        if (t < 1.0f / detail::kBounceD1) {
            return detail::kBounceN1 * t * t;
        }
        if (t < 2.0f / detail::kBounceD1) {
            float u = t - 1.5f / detail::kBounceD1;
            return detail::kBounceN1 * u * u + 0.75f;
        }
        if (t < 2.5f / detail::kBounceD1) {
            float u = t - 2.25f / detail::kBounceD1;
            return detail::kBounceN1 * u * u + 0.9375f;
        }
        float u = t - 2.625f / detail::kBounceD1;
        return detail::kBounceN1 * u * u + 0.984375f;
    }

    constexpr float inBounce(float t) {
        return 1.0f - outBounce(1.0f - t);
    }

    constexpr float inOutBounce(float t) {
        if (t < 0.5f) return (1.0f - outBounce(1.0f - 2.0f * t)) * 0.5f;
        return (1.0f + outBounce(2.0f * t - 1.0f)) * 0.5f;
    }

    // --- Smoothstep (Hermite / Ken Perlin, clamped to [0, 1]) ---

    constexpr float smoothstep(float t) {
        float x = detail::clamp01(t);
        return x * x * (3.0f - 2.0f * x);
    }

    constexpr float smootherstep(float t) {
        float x = detail::clamp01(t);
        return x * x * x * (x * (x * 6.0f - 15.0f) + 10.0f);
    }

    // --- Runtime dispatch and interpolation helpers ---

    constexpr EaseFunc function(EaseType type) {
        switch (type) {
        case EaseType::Linear: return linear;
        case EaseType::InSine: return inSine;
        case EaseType::OutSine: return outSine;
        case EaseType::InOutSine: return inOutSine;
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
        case EaseType::Smoothstep: return smoothstep;
        case EaseType::Smootherstep: return smootherstep;
        }
        return linear;
    }

    inline float evaluate(EaseType type, float t) {
        return function(type)(t);
    }

    // Linear interpolation between `from` and `to` (works with scalars and GLM vectors).
    template <typename T> constexpr T lerp(const T& from, const T& to, float t) {
        return from + (to - from) * t;
    }

    template <typename T> inline T lerp(const T& from, const T& to, float t, EaseFunc easeFunc) {
        return lerp(from, to, easeFunc ? easeFunc(t) : t);
    }

    template <typename T> inline T lerp(const T& from, const T& to, float t, EaseType type) {
        return lerp(from, to, evaluate(type, t));
    }
}
