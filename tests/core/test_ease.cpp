#include <doctest/doctest.h>
#include <mge/core/math/Ease.h>

#include <glm/glm.hpp>

#include <array>
#include <cmath>

namespace {
    using namespace mge::ease;

    struct NamedEase {
        const char* name;
        EaseType type;
        EaseFunc fn;
        bool monotonic;
        bool overshoots;
    };

    constexpr std::array<NamedEase, 33> kAllEases = {{
        {"linear", EaseType::Linear, linear, true, false},
        {"inSine", EaseType::InSine, inSine, true, false},
        {"outSine", EaseType::OutSine, outSine, true, false},
        {"inOutSine", EaseType::InOutSine, inOutSine, true, false},
        {"inQuad", EaseType::InQuad, inQuad, true, false},
        {"outQuad", EaseType::OutQuad, outQuad, true, false},
        {"inOutQuad", EaseType::InOutQuad, inOutQuad, true, false},
        {"inCubic", EaseType::InCubic, inCubic, true, false},
        {"outCubic", EaseType::OutCubic, outCubic, true, false},
        {"inOutCubic", EaseType::InOutCubic, inOutCubic, true, false},
        {"inQuart", EaseType::InQuart, inQuart, true, false},
        {"outQuart", EaseType::OutQuart, outQuart, true, false},
        {"inOutQuart", EaseType::InOutQuart, inOutQuart, true, false},
        {"inQuint", EaseType::InQuint, inQuint, true, false},
        {"outQuint", EaseType::OutQuint, outQuint, true, false},
        {"inOutQuint", EaseType::InOutQuint, inOutQuint, true, false},
        {"inExpo", EaseType::InExpo, inExpo, true, false},
        {"outExpo", EaseType::OutExpo, outExpo, true, false},
        {"inOutExpo", EaseType::InOutExpo, inOutExpo, true, false},
        {"inCirc", EaseType::InCirc, inCirc, true, false},
        {"outCirc", EaseType::OutCirc, outCirc, true, false},
        {"inOutCirc", EaseType::InOutCirc, inOutCirc, true, false},
        {"inBack", EaseType::InBack, inBack, false, true},
        {"outBack", EaseType::OutBack, outBack, false, true},
        {"inOutBack", EaseType::InOutBack, inOutBack, false, true},
        {"inElastic", EaseType::InElastic, inElastic, false, true},
        {"outElastic", EaseType::OutElastic, outElastic, false, true},
        {"inOutElastic", EaseType::InOutElastic, inOutElastic, false, true},
        {"inBounce", EaseType::InBounce, inBounce, false, false},
        {"outBounce", EaseType::OutBounce, outBounce, false, false},
        {"inOutBounce", EaseType::InOutBounce, inOutBounce, false, false},
        {"smoothstep", EaseType::Smoothstep, smoothstep, true, false},
        {"smootherstep", EaseType::Smootherstep, smootherstep, true, false},
    }};

    struct EaseFamily {
        const char* name;
        EaseFunc inFn;
        EaseFunc outFn;
        EaseFunc inOutFn;
    };

    constexpr std::array<EaseFamily, 10> kFamilies = {{
        {"Sine", inSine, outSine, inOutSine},
        {"Quad", inQuad, outQuad, inOutQuad},
        {"Cubic", inCubic, outCubic, inOutCubic},
        {"Quart", inQuart, outQuart, inOutQuart},
        {"Quint", inQuint, outQuint, inOutQuint},
        {"Expo", inExpo, outExpo, inOutExpo},
        {"Circ", inCirc, outCirc, inOutCirc},
        {"Back", inBack, outBack, inOutBack},
        {"Elastic", inElastic, outElastic, inOutElastic},
        {"Bounce", inBounce, outBounce, inOutBounce},
    }};
}

TEST_CASE("Ease: endpoints f(0) == 0, f(1) == 1, and EaseType dispatch match") {
    for (const NamedEase& entry : kAllEases) {
        CAPTURE(entry.name);
        CHECK(function(entry.type) == entry.fn);
        CHECK(entry.fn(0.0f) == doctest::Approx(0.0f).epsilon(1e-5f));
        CHECK(entry.fn(1.0f) == doctest::Approx(1.0f).epsilon(1e-5f));
        CHECK(evaluate(entry.type, 0.0f) == doctest::Approx(0.0f).epsilon(1e-5f));
        CHECK(evaluate(entry.type, 1.0f) == doctest::Approx(1.0f).epsilon(1e-5f));
        CHECK(evaluate(entry.type, 0.37f) == doctest::Approx(entry.fn(0.37f)).epsilon(1e-6f));
    }
}

TEST_CASE("Ease: In/Out duality and InOut symmetry") {
    constexpr int kSteps = 20;
    for (const EaseFamily& family : kFamilies) {
        CAPTURE(family.name);
        CHECK(family.inOutFn(0.5f) == doctest::Approx(0.5f).epsilon(1e-5f));

        for (int i = 0; i <= kSteps; i++) {
            const float t = static_cast<float>(i) / static_cast<float>(kSteps);
            CAPTURE(t);
            // out(t) == 1 - in(1 - t)
            CHECK(family.outFn(t) == doctest::Approx(1.0f - family.inFn(1.0f - t)).epsilon(1e-4f));
            // inOut(1 - t) == 1 - inOut(t)
            CHECK(family.inOutFn(1.0f - t) == doctest::Approx(1.0f - family.inOutFn(t)).epsilon(1e-4f));
        }
    }

    CHECK(smoothstep(0.5f) == doctest::Approx(0.5f));
    CHECK(smootherstep(0.5f) == doctest::Approx(0.5f));
    for (int i = 0; i <= kSteps; i++) {
        const float t = static_cast<float>(i) / static_cast<float>(kSteps);
        CHECK(smoothstep(1.0f - t) == doctest::Approx(1.0f - smoothstep(t)).epsilon(1e-5f));
        CHECK(smootherstep(1.0f - t) == doctest::Approx(1.0f - smootherstep(t)).epsilon(1e-5f));
    }
}

TEST_CASE("Ease: monotonicity, bounds, and overshoot behavior") {
    constexpr int kSamples = 100;
    for (const NamedEase& entry : kAllEases) {
        CAPTURE(entry.name);
        float prev = entry.fn(0.0f);
        bool sawOvershoot = false;

        for (int i = 1; i <= kSamples; i++) {
            const float t = static_cast<float>(i) / static_cast<float>(kSamples);
            const float value = entry.fn(t);
            CHECK(std::isfinite(value));

            if (entry.monotonic) {
                CHECK(value >= prev - 1e-5f);
            }
            if (!entry.overshoots) {
                CHECK(value >= -1e-5f);
                CHECK(value <= 1.0f + 1e-5f);
            } else if (value < -1e-3f || value > 1.0f + 1e-3f) {
                sawOvershoot = true;
            }
            prev = value;
        }

        if (entry.overshoots) {
            CHECK(sawOvershoot);
        }
    }
}

TEST_CASE("Ease: known reference values and edge safety") {
    CHECK(linear(0.25f) == doctest::Approx(0.25f));
    CHECK(inQuad(0.5f) == doctest::Approx(0.25f));
    CHECK(outQuad(0.5f) == doctest::Approx(0.75f));
    CHECK(inCubic(0.5f) == doctest::Approx(0.125f));
    CHECK(outCubic(0.5f) == doctest::Approx(0.875f));
    CHECK(inQuart(0.5f) == doctest::Approx(0.0625f));
    CHECK(inQuint(0.5f) == doctest::Approx(0.03125f));
    CHECK(inExpo(0.5f) == doctest::Approx(0.03125f));
    CHECK(outExpo(0.5f) == doctest::Approx(0.96875f));
    CHECK(inCirc(0.5f) == doctest::Approx(1.0f - std::sqrt(0.75f)));
    CHECK(outCirc(0.5f) == doctest::Approx(std::sqrt(0.75f)));

    // Back and Elastic overshoot in expected directions
    CHECK(inBack(0.25f) < 0.0f);
    CHECK(outBack(0.75f) > 1.0f);
    CHECK(inOutBack(0.15f) < 0.0f);
    CHECK(inOutBack(0.85f) > 1.0f);

    // Out-of-range inputs do not produce NaN for sqrt/smoothstep/expo/elastic curves
    CHECK(std::isfinite(inCirc(-0.1f)));
    CHECK(std::isfinite(inCirc(1.1f)));
    CHECK(std::isfinite(outCirc(-0.1f)));
    CHECK(std::isfinite(outCirc(1.1f)));
    CHECK(std::isfinite(inOutCirc(-0.1f)));
    CHECK(std::isfinite(inOutCirc(1.1f)));
    CHECK(inExpo(-0.5f) == 0.0f);
    CHECK(outExpo(1.5f) == 1.0f);
    CHECK(inOutExpo(-0.5f) == 0.0f);
    CHECK(inOutExpo(1.5f) == 1.0f);
    CHECK(inElastic(-0.5f) == 0.0f);
    CHECK(inElastic(1.5f) == 1.0f);
    CHECK(outElastic(-0.5f) == 0.0f);
    CHECK(outElastic(1.5f) == 1.0f);
    CHECK(inOutElastic(-0.5f) == 0.0f);
    CHECK(inOutElastic(1.5f) == 1.0f);
    CHECK(smoothstep(-0.5f) == 0.0f);
    CHECK(smoothstep(1.5f) == 1.0f);
    CHECK(smootherstep(-0.5f) == 0.0f);
    CHECK(smootherstep(1.5f) == 1.0f);
}

TEST_CASE("Ease: lerp with scalars, vectors, EaseFunc, and EaseType") {
    CHECK(lerp(10.0f, 20.0f, 0.5f) == doctest::Approx(15.0f));
    CHECK(lerp(10.0f, 20.0f, 0.5f, inQuad) == doctest::Approx(12.5f));
    CHECK(lerp(10.0f, 20.0f, 0.5f, EaseType::OutQuad) == doctest::Approx(17.5f));
    CHECK(lerp(10.0f, 20.0f, 0.5f, static_cast<EaseFunc>(nullptr)) == doctest::Approx(15.0f));

    const glm::vec3 start(0.0f, 10.0f, -4.0f);
    const glm::vec3 end(8.0f, 10.0f, 4.0f);
    const glm::vec3 mid = lerp(start, end, 0.5f, EaseType::InQuad);
    CHECK(mid.x == doctest::Approx(2.0f));
    CHECK(mid.y == doctest::Approx(10.0f));
    CHECK(mid.z == doctest::Approx(-2.0f));
}

TEST_CASE("Ease: constexpr evaluation for polynomial, back, bounce, and smoothstep") {
    constexpr float kLin = linear(0.5f);
    constexpr float kQuad = inQuad(0.5f);
    constexpr float kCubic = outCubic(1.0f);
    constexpr float kQuart = inOutQuart(0.5f);
    constexpr float kQuint = inQuint(0.0f);
    constexpr float kBack = inBack(1.0f);
    constexpr float kBounce = outBounce(1.0f);
    constexpr float kSmooth = smoothstep(0.5f);
    constexpr float kSmoother = smootherstep(0.5f);
    constexpr float kLerped = lerp(2.0f, 6.0f, 0.25f);
    constexpr EaseFunc kFn = function(EaseType::InOutCubic);

    static_assert(kLin == 0.5f);
    static_assert(kQuad == 0.25f);
    static_assert(kCubic == 1.0f);
    static_assert(kQuart == 0.5f);
    static_assert(kQuint == 0.0f);
    static_assert(kBack > 0.999f && kBack < 1.001f);
    static_assert(kBounce > 0.999f && kBounce < 1.001f);
    static_assert(kSmooth == 0.5f);
    static_assert(kSmoother == 0.5f);
    static_assert(kLerped == 3.0f);
    static_assert(kFn == inOutCubic);

    CHECK(kFn(0.5f) == doctest::Approx(0.5f));
}
