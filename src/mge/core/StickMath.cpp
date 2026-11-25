#include <mge/core/StickMath.h>

#include <algorithm>
#include <cmath>

namespace mge {

    namespace {
        constexpr float kInt16PosMax = 32767.0f;
        constexpr float kInt16NegMax = 32768.0f;

        bool validDeadzoneRange(float inner, float outer) {
            return std::isfinite(inner) && std::isfinite(outer) && inner < outer;
        }

        float evaluateCurve01(float t, ResponseCurve curve, float exponent) {
            switch (curve) {
                case ResponseCurve::Linear: return t;
                case ResponseCurve::Quadratic: return t * t;
                case ResponseCurve::Cubic: return t * t * t;
                case ResponseCurve::Power:
                    if (!std::isfinite(exponent) || exponent <= 0.0f) return t;
                    return std::pow(t, exponent);
                case ResponseCurve::SCurve: return t * t * (3.0f - 2.0f * t);
            }
            return t;
        }
    }

    float normalizeAxisInt16(std::int16_t raw) {
        if (raw >= 0) return static_cast<float>(raw) / kInt16PosMax;
        return static_cast<float>(raw) / kInt16NegMax;
    }

    float normalizeTriggerInt16(std::int16_t raw) {
        if (raw <= 0) return 0.0f;
        return static_cast<float>(raw) / kInt16PosMax;
    }

    float applyAxisDeadzone(float value, float inner, float outer) {
        if (!std::isfinite(value) || !std::isfinite(inner) || !std::isfinite(outer)) return 0.0f;
        const float clampedInner = std::clamp(inner, 0.0f, 1.0f);
        const float clampedOuter = std::clamp(outer, 0.0f, 1.0f);
        if (!validDeadzoneRange(clampedInner, clampedOuter)) return 0.0f;

        const float magnitude = std::abs(value);
        if (magnitude <= clampedInner) return 0.0f;

        const float rescaled =
            std::clamp((magnitude - clampedInner) / (clampedOuter - clampedInner), 0.0f, 1.0f);
        return value < 0.0f ? -rescaled : rescaled;
    }

    float applyTriggerDeadzone(float value, float inner, float outer) {
        if (!std::isfinite(value) || !std::isfinite(inner) || !std::isfinite(outer) || value <= 0.0f) {
            return 0.0f;
        }
        const float clampedInner = std::clamp(inner, 0.0f, 1.0f);
        const float clampedOuter = std::clamp(outer, 0.0f, 1.0f);
        if (!validDeadzoneRange(clampedInner, clampedOuter)) return 0.0f;

        if (value <= clampedInner) return 0.0f;
        return std::clamp((value - clampedInner) / (clampedOuter - clampedInner), 0.0f, 1.0f);
    }

    float applyResponseCurve(float value, ResponseCurve curve, float exponent) {
        if (!std::isfinite(value) || value == 0.0f) return 0.0f;
        const float magnitude = std::clamp(std::abs(value), 0.0f, 1.0f);
        const float curved = evaluateCurve01(magnitude, curve, exponent);
        if (curved == 0.0f) return 0.0f;
        return value < 0.0f ? -curved : curved;
    }

    glm::vec2 applyStickDeadzone(const glm::vec2& raw, float inner, float outer, DeadzoneShape shape) {
        if (!std::isfinite(raw.x) || !std::isfinite(raw.y) || !std::isfinite(inner) ||
            !std::isfinite(outer)) {
            return glm::vec2(0.0f);
        }
        const float clampedInner = std::clamp(inner, 0.0f, 1.0f);
        const float clampedOuter = std::clamp(outer, 0.0f, 1.0f);
        if (!validDeadzoneRange(clampedInner, clampedOuter)) return glm::vec2(0.0f);

        switch (shape) {
            case DeadzoneShape::Axial:
                return glm::vec2(applyAxisDeadzone(raw.x, clampedInner, clampedOuter),
                                 applyAxisDeadzone(raw.y, clampedInner, clampedOuter));

            case DeadzoneShape::Radial: {
                const float length = glm::length(raw);
                if (length <= clampedInner) return glm::vec2(0.0f);
                const float rescaled =
                    std::clamp((length - clampedInner) / (clampedOuter - clampedInner), 0.0f, 1.0f);
                return (raw / length) * rescaled;
            }

            case DeadzoneShape::Hybrid: {
                const float length = glm::length(raw);
                if (length <= clampedInner) return glm::vec2(0.0f);
                glm::vec2 axial(applyAxisDeadzone(raw.x, clampedInner, clampedOuter),
                                applyAxisDeadzone(raw.y, clampedInner, clampedOuter));
                const float axialLength = glm::length(axial);
                if (axialLength > 1.0f) axial /= axialLength;
                return axial;
            }
        }
        return glm::vec2(0.0f);
    }

    glm::vec2 applyStickResponseCurve(const glm::vec2& stick, ResponseCurve curve, float exponent) {
        if (!std::isfinite(stick.x) || !std::isfinite(stick.y)) return glm::vec2(0.0f);
        const float length = glm::length(stick);
        if (length <= 0.0f) return glm::vec2(0.0f);
        const float clampedLength = std::min(length, 1.0f);
        const float curvedLength = evaluateCurve01(clampedLength, curve, exponent);
        if (curvedLength == 0.0f) return glm::vec2(0.0f);
        return (stick / length) * curvedLength;
    }

    glm::vec2 squareToCircle(const glm::vec2& square) {
        if (!std::isfinite(square.x) || !std::isfinite(square.y)) return glm::vec2(0.0f);
        const float x = std::clamp(square.x, -1.0f, 1.0f);
        const float y = std::clamp(square.y, -1.0f, 1.0f);
        return glm::vec2(x * std::sqrt(1.0f - 0.5f * y * y), y * std::sqrt(1.0f - 0.5f * x * x));
    }

    glm::vec2 processStick(const glm::vec2& raw, const StickConfig& config) {
        glm::vec2 result =
            applyStickDeadzone(raw, config.innerDeadzone, config.outerDeadzone, config.deadzoneShape);

        if (config.deadzoneShape == DeadzoneShape::Axial) {
            result.x = applyResponseCurve(result.x, config.curve, config.curveExponent);
            result.y = applyResponseCurve(result.y, config.curve, config.curveExponent);
        } else {
            result = applyStickResponseCurve(result, config.curve, config.curveExponent);
        }

        if (config.invertX && result.x != 0.0f) result.x = -result.x;
        if (config.invertY && result.y != 0.0f) result.y = -result.y;
        return result;
    }
}
