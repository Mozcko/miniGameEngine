#pragma once

#include <glm/glm.hpp>

#include <cstdint>

namespace mge {

    // Deadzone shape applied to a 2D analog stick.
    enum class DeadzoneShape {
        Radial, // Circular deadzone; preserves direction and clamps magnitude to [0, 1]
        Axial,  // Independent per-axis deadzone; components stay in [-1, 1]
        Hybrid  // Radial + per-axis snap near cardinals; clamps magnitude to [0, 1]
    };

    // Response curve shaping analog input sensitivity after deadzone rescaling.
    enum class ResponseCurve {
        Linear,    // f(t) = t
        Quadratic, // f(t) = t^2
        Cubic,     // f(t) = t^3
        Power,     // f(t) = t^exponent (configured via StickConfig::curveExponent)
        SCurve     // Smoothstep: f(t) = t^2 * (3 - 2t)
    };

    // Full configuration for processing a 2D analog stick.
    struct StickConfig {
        float innerDeadzone = 0.15f;
        float outerDeadzone = 0.95f;
        DeadzoneShape deadzoneShape = DeadzoneShape::Radial;
        ResponseCurve curve = ResponseCurve::Linear;
        float curveExponent = 1.0f; // Used when curve == ResponseCurve::Power (must be > 0)
        bool invertX = false;
        bool invertY = false;
    };

    // Converts a signed 16-bit gamepad axis (e.g., SDL Sint16 in [-32768, 32767]) to [-1, 1].
    float normalizeAxisInt16(std::int16_t raw);

    // Converts a signed 16-bit trigger value ([0, 32767], negatives clamped to 0) to [0, 1].
    float normalizeTriggerInt16(std::int16_t raw);

    // Applies a symmetric deadzone to a 1D signed axis in [-1, 1] and rescales [inner, outer] to [0, 1].
    // Preserves sign; non-finite values or inner >= outer return 0.
    float applyAxisDeadzone(float value, float inner, float outer = 1.0f);

    // Applies a deadzone to a unipolar trigger in [0, 1] and rescales [inner, outer] to [0, 1].
    // Negative or non-finite values, or inner >= outer, return 0.
    float applyTriggerDeadzone(float value, float inner, float outer = 1.0f);

    // Applies a response curve to a signed scalar in [-1, 1] (or unipolar [0, 1]), preserving sign.
    // Input magnitude is clamped to [0, 1]. Non-positive/non-finite Power exponents fall back to Linear.
    float applyResponseCurve(float value, ResponseCurve curve, float exponent = 1.0f);

    // Applies the selected 2D deadzone to a raw stick vector.
    // Radial and Hybrid clamp output magnitude to [0, 1]; Axial outputs components in [-1, 1].
    glm::vec2 applyStickDeadzone(const glm::vec2& raw, float inner, float outer = 1.0f,
                                 DeadzoneShape shape = DeadzoneShape::Radial);

    // Applies a response curve radially to a 2D stick vector, preserving its direction.
    // Magnitude is clamped to [0, 1] before applying the curve.
    glm::vec2 applyStickResponseCurve(const glm::vec2& stick, ResponseCurve curve, float exponent = 1.0f);

    // Maps a square-gated stick vector in [-1, 1]^2 smoothly onto the unit disk (elliptical mapping).
    glm::vec2 squareToCircle(const glm::vec2& square);

    // Full stick pipeline: deadzone -> response curve -> axis inversion.
    // Uses per-axis curve for Axial deadzone, and direction-preserving radial curve for Radial/Hybrid.
    glm::vec2 processStick(const glm::vec2& raw, const StickConfig& config = {});
}
