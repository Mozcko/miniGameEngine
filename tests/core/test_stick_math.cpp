#include <doctest/doctest.h>
#include <mge/core/StickMath.h>

#include <cmath>
#include <limits>

using namespace mge;

TEST_CASE("Int16 axis and trigger normalization") {
    CHECK(normalizeAxisInt16(0) == doctest::Approx(0.0f));
    CHECK(normalizeAxisInt16(32767) == doctest::Approx(1.0f));
    CHECK(normalizeAxisInt16(-32768) == doctest::Approx(-1.0f));
    CHECK(normalizeAxisInt16(-16384) == doctest::Approx(-0.5f));

    CHECK(normalizeTriggerInt16(0) == doctest::Approx(0.0f));
    CHECK(normalizeTriggerInt16(-1000) == doctest::Approx(0.0f));
    CHECK(normalizeTriggerInt16(32767) == doctest::Approx(1.0f));
    CHECK(normalizeTriggerInt16(16384) == doctest::Approx(16384.0f / 32767.0f));
}

TEST_CASE("1D axis deadzone: symmetric rescaling, saturation, and edge cases") {
    CHECK(applyAxisDeadzone(0.0f, 0.2f, 0.8f) == doctest::Approx(0.0f));
    CHECK(applyAxisDeadzone(0.2f, 0.2f, 0.8f) == doctest::Approx(0.0f));
    CHECK(applyAxisDeadzone(-0.19f, 0.2f, 0.8f) == doctest::Approx(0.0f));

    CHECK(applyAxisDeadzone(0.5f, 0.2f, 0.8f) == doctest::Approx(0.5f));
    CHECK(applyAxisDeadzone(-0.5f, 0.2f, 0.8f) == doctest::Approx(-0.5f));

    CHECK(applyAxisDeadzone(0.8f, 0.2f, 0.8f) == doctest::Approx(1.0f));
    CHECK(applyAxisDeadzone(1.0f, 0.2f, 0.8f) == doctest::Approx(1.0f));
    CHECK(applyAxisDeadzone(-0.95f, 0.2f, 0.8f) == doctest::Approx(-1.0f));

    // Invalid ranges and non-finite inputs return 0
    const float nanValue = std::numeric_limits<float>::quiet_NaN();
    const float infValue = std::numeric_limits<float>::infinity();
    CHECK(applyAxisDeadzone(0.5f, 0.8f, 0.2f) == doctest::Approx(0.0f));
    CHECK(applyAxisDeadzone(0.5f, 0.5f, 0.5f) == doctest::Approx(0.0f));
    CHECK(applyAxisDeadzone(nanValue, 0.1f, 0.9f) == doctest::Approx(0.0f));
    CHECK(applyAxisDeadzone(infValue, 0.1f, 0.9f) == doctest::Approx(0.0f));
    CHECK(applyAxisDeadzone(0.5f, nanValue, 0.9f) == doctest::Approx(0.0f));
}

TEST_CASE("1D trigger deadzone: unipolar rescaling and clamping") {
    CHECK(applyTriggerDeadzone(-0.5f, 0.1f, 0.9f) == doctest::Approx(0.0f));
    CHECK(applyTriggerDeadzone(0.1f, 0.1f, 0.9f) == doctest::Approx(0.0f));
    CHECK(applyTriggerDeadzone(0.5f, 0.1f, 0.9f) == doctest::Approx(0.5f));
    CHECK(applyTriggerDeadzone(0.9f, 0.1f, 0.9f) == doctest::Approx(1.0f));
    CHECK(applyTriggerDeadzone(1.2f, 0.1f, 0.9f) == doctest::Approx(1.0f));

    const float nanValue = std::numeric_limits<float>::quiet_NaN();
    CHECK(applyTriggerDeadzone(nanValue, 0.1f, 0.9f) == doctest::Approx(0.0f));
    CHECK(applyTriggerDeadzone(0.5f, 0.9f, 0.1f) == doctest::Approx(0.0f));
}

TEST_CASE("Response curves: endpoints, symmetry, monotonicity, and fallback") {
    const ResponseCurve curves[] = {ResponseCurve::Linear, ResponseCurve::Quadratic, ResponseCurve::Cubic,
                                    ResponseCurve::Power, ResponseCurve::SCurve};
    for (ResponseCurve curve : curves) {
        const float exponent = (curve == ResponseCurve::Power) ? 2.5f : 1.0f;
        CHECK(applyResponseCurve(0.0f, curve, exponent) == doctest::Approx(0.0f));
        CHECK(applyResponseCurve(1.0f, curve, exponent) == doctest::Approx(1.0f));
        CHECK(applyResponseCurve(-1.0f, curve, exponent) == doctest::Approx(-1.0f));
        CHECK(applyResponseCurve(1.5f, curve, exponent) == doctest::Approx(1.0f));

        // Strict monotonicity and odd symmetry on (0, 1)
        float prev = 0.0f;
        for (int i = 1; i <= 10; ++i) {
            const float x = static_cast<float>(i) * 0.1f;
            const float pos = applyResponseCurve(x, curve, exponent);
            const float neg = applyResponseCurve(-x, curve, exponent);
            CHECK(pos > prev);
            CHECK(neg == doctest::Approx(-pos));
            prev = pos;
        }
    }

    CHECK(applyResponseCurve(0.5f, ResponseCurve::Linear) == doctest::Approx(0.5f));
    CHECK(applyResponseCurve(0.5f, ResponseCurve::Quadratic) == doctest::Approx(0.25f));
    CHECK(applyResponseCurve(0.5f, ResponseCurve::Cubic) == doctest::Approx(0.125f));
    CHECK(applyResponseCurve(0.25f, ResponseCurve::Power, 0.5f) == doctest::Approx(0.5f));
    CHECK(applyResponseCurve(0.5f, ResponseCurve::SCurve) == doctest::Approx(0.5f));
    CHECK(applyResponseCurve(0.25f, ResponseCurve::SCurve) == doctest::Approx(0.15625f));

    // Non-positive or NaN Power exponent falls back safely to Linear
    const float nanValue = std::numeric_limits<float>::quiet_NaN();
    CHECK(applyResponseCurve(0.4f, ResponseCurve::Power, 0.0f) == doctest::Approx(0.4f));
    CHECK(applyResponseCurve(0.4f, ResponseCurve::Power, -2.0f) == doctest::Approx(0.4f));
    CHECK(applyResponseCurve(0.4f, ResponseCurve::Power, nanValue) == doctest::Approx(0.4f));
    CHECK(applyResponseCurve(nanValue, ResponseCurve::Quadratic) == doctest::Approx(0.0f));
}

TEST_CASE("2D stick deadzone: Radial preserves direction and rescales smoothly") {
    // Inside deadzone
    CHECK(applyStickDeadzone({0.1f, 0.1f}, 0.2f, 1.0f, DeadzoneShape::Radial) == glm::vec2(0.0f));

    // Midpoint along diagonal: length = 0.6 -> rescaled = (0.6 - 0.2) / (1.0 - 0.2) = 0.5
    const glm::vec2 dir = glm::normalize(glm::vec2(3.0f, 4.0f));
    const glm::vec2 mid = applyStickDeadzone(dir * 0.6f, 0.2f, 1.0f, DeadzoneShape::Radial);
    CHECK(glm::length(mid) == doctest::Approx(0.5f));
    CHECK(glm::normalize(mid).x == doctest::Approx(dir.x));
    CHECK(glm::normalize(mid).y == doctest::Approx(dir.y));

    // Saturation at outer deadzone and clamping past unit circle
    const glm::vec2 sat = applyStickDeadzone(glm::vec2(1.0f, 1.0f), 0.15f, 0.9f, DeadzoneShape::Radial);
    CHECK(glm::length(sat) == doctest::Approx(1.0f));
    CHECK(sat.x == doctest::Approx(sat.y));

    // Non-finite inputs return zero
    const float nanValue = std::numeric_limits<float>::quiet_NaN();
    CHECK(applyStickDeadzone({nanValue, 0.5f}, 0.1f, 1.0f, DeadzoneShape::Radial) == glm::vec2(0.0f));
}

TEST_CASE("2D stick deadzone: Axial treats axes independently") {
    const glm::vec2 snapped = applyStickDeadzone({0.6f, 0.1f}, 0.2f, 1.0f, DeadzoneShape::Axial);
    CHECK(snapped.x == doctest::Approx(0.5f));
    CHECK(snapped.y == doctest::Approx(0.0f));

    const glm::vec2 corner = applyStickDeadzone({1.0f, -1.0f}, 0.2f, 0.9f, DeadzoneShape::Axial);
    CHECK(corner.x == doctest::Approx(1.0f));
    CHECK(corner.y == doctest::Approx(-1.0f));
}

TEST_CASE("2D stick deadzone: Hybrid snaps near cardinals and clamps diagonals to unit disk") {
    // Inside deadzone
    CHECK(applyStickDeadzone({0.1f, 0.1f}, 0.2f, 1.0f, DeadzoneShape::Hybrid) == glm::vec2(0.0f));

    // Near horizontal axis: Y snaps to 0 while X rescales
    const glm::vec2 snapped = applyStickDeadzone({0.6f, 0.1f}, 0.2f, 1.0f, DeadzoneShape::Hybrid);
    CHECK(snapped.x == doctest::Approx(0.5f));
    CHECK(snapped.y == doctest::Approx(0.0f));

    // Full diagonal is clamped to unit circle (never > 1)
    const glm::vec2 diag = applyStickDeadzone({1.0f, 1.0f}, 0.2f, 0.9f, DeadzoneShape::Hybrid);
    CHECK(glm::length(diag) == doctest::Approx(1.0f));
    CHECK(diag.x == doctest::Approx(diag.y));
}

TEST_CASE("2D stick response curve preserves stick angle on arbitrary diagonals") {
    const glm::vec2 dir = glm::normalize(glm::vec2(0.3f, 0.95f));
    const glm::vec2 input = dir * 0.5f;
    const glm::vec2 curved = applyStickResponseCurve(input, ResponseCurve::Quadratic);
    CHECK(glm::length(curved) == doctest::Approx(0.25f));
    CHECK(glm::normalize(curved).x == doctest::Approx(dir.x));
    CHECK(glm::normalize(curved).y == doctest::Approx(dir.y));

    // Magnitude > 1 is clamped to 1
    const glm::vec2 clamped = applyStickResponseCurve({2.0f, 0.0f}, ResponseCurve::Cubic);
    CHECK(clamped.x == doctest::Approx(1.0f));
    CHECK(clamped.y == doctest::Approx(0.0f));

    CHECK(applyStickResponseCurve({0.0f, 0.0f}, ResponseCurve::Quadratic) == glm::vec2(0.0f));
}

TEST_CASE("squareToCircle maps [-1, 1]^2 smoothly onto the unit disk") {
    CHECK(squareToCircle({0.0f, 0.0f}) == glm::vec2(0.0f));

    // Cardinals unchanged
    const glm::vec2 right = squareToCircle({1.0f, 0.0f});
    CHECK(right.x == doctest::Approx(1.0f));
    CHECK(right.y == doctest::Approx(0.0f));
    const glm::vec2 down = squareToCircle({0.0f, -1.0f});
    CHECK(down.x == doctest::Approx(0.0f));
    CHECK(down.y == doctest::Approx(-1.0f));

    // Corners map onto the unit circle at 45 degrees
    const glm::vec2 corner = squareToCircle({1.0f, 1.0f});
    CHECK(glm::length(corner) == doctest::Approx(1.0f));
    CHECK(corner.x == doctest::Approx(std::sqrt(0.5f)));
    CHECK(corner.y == doctest::Approx(std::sqrt(0.5f)));

    // Out-of-range inputs are clamped first
    const glm::vec2 clamped = squareToCircle({2.0f, -2.0f});
    CHECK(glm::length(clamped) == doctest::Approx(1.0f));

    const float nanValue = std::numeric_limits<float>::quiet_NaN();
    CHECK(squareToCircle({nanValue, 0.5f}) == glm::vec2(0.0f));
}

TEST_CASE("processStick combines deadzone, response curve, and axis inversion") {
    StickConfig cfg;
    cfg.innerDeadzone = 0.2f;
    cfg.outerDeadzone = 1.0f;
    cfg.deadzoneShape = DeadzoneShape::Radial;
    cfg.curve = ResponseCurve::Quadratic;
    cfg.invertY = true;

    // Inside deadzone -> exact +0.0f (no -0.0f from inversion)
    const glm::vec2 zero = processStick({0.05f, 0.05f}, cfg);
    CHECK(zero.x == 0.0f);
    CHECK(zero.y == 0.0f);
    CHECK_FALSE(std::signbit(zero.y));

    // Halfway from inner to outer along (3, 4): rescaled 0.5 -> quadratic 0.25 -> Y inverted
    const glm::vec2 dir = glm::normalize(glm::vec2(3.0f, 4.0f));
    const glm::vec2 out = processStick(dir * 0.6f, cfg);
    CHECK(glm::length(out) == doctest::Approx(0.25f));
    CHECK(out.x == doctest::Approx(dir.x * 0.25f));
    CHECK(out.y == doctest::Approx(-dir.y * 0.25f));

    // Axial mode applies curve per-axis
    StickConfig axialCfg;
    axialCfg.innerDeadzone = 0.0f;
    axialCfg.outerDeadzone = 1.0f;
    axialCfg.deadzoneShape = DeadzoneShape::Axial;
    axialCfg.curve = ResponseCurve::Cubic;
    axialCfg.invertX = true;

    const glm::vec2 axialOut = processStick({0.5f, 1.0f}, axialCfg);
    CHECK(axialOut.x == doctest::Approx(-0.125f));
    CHECK(axialOut.y == doctest::Approx(1.0f));
}
