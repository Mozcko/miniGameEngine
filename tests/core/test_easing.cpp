#include <doctest/doctest.h>
#include <mge/core/Easing.h>

#include <array>
#include <cmath>

using namespace mge;

TEST_CASE("All EaseType curves hit exact endpoints and stay finite") {
    constexpr std::array<EaseType, 33> kAllTypes = {
        EaseType::Linear,       EaseType::InQuad,      EaseType::OutQuad,     EaseType::InOutQuad,
        EaseType::InCubic,      EaseType::OutCubic,    EaseType::InOutCubic,  EaseType::InQuart,
        EaseType::OutQuart,     EaseType::InOutQuart,  EaseType::InQuint,     EaseType::OutQuint,
        EaseType::InOutQuint,   EaseType::InSine,      EaseType::OutSine,     EaseType::InOutSine,
        EaseType::InExpo,       EaseType::OutExpo,     EaseType::InOutExpo,   EaseType::InCirc,
        EaseType::OutCirc,      EaseType::InOutCirc,   EaseType::InBack,      EaseType::OutBack,
        EaseType::InOutBack,    EaseType::InElastic,   EaseType::OutElastic,  EaseType::InOutElastic,
        EaseType::InBounce,     EaseType::OutBounce,   EaseType::InOutBounce, EaseType::SmoothStep,
        EaseType::SmootherStep,
    };

    for (EaseType type : kAllTypes) {
        REQUIRE(ease::fromType(type) != nullptr);
        CHECK(ease::evaluate(type, 0.0f) == doctest::Approx(0.0f).epsilon(1e-5));
        CHECK(ease::evaluate(type, 1.0f) == doctest::Approx(1.0f).epsilon(1e-5));
        CHECK(ease::evaluate(type, -0.5f) == doctest::Approx(0.0f).epsilon(1e-5));
        CHECK(ease::evaluate(type, 1.5f) == doctest::Approx(1.0f).epsilon(1e-5));

        for (int i = 0; i <= 20; i++) {
            const float t = static_cast<float>(i) / 20.0f;
            const float v = ease::evaluate(type, t);
            CHECK(std::isfinite(v));
        }
    }
}

TEST_CASE("Polynomial, sine, expo, circ, and smoothstep known values") {
    CHECK(ease::linear(0.25f) == doctest::Approx(0.25f));
    CHECK(ease::inQuad(0.5f) == doctest::Approx(0.25f));
    CHECK(ease::outQuad(0.5f) == doctest::Approx(0.75f));
    CHECK(ease::inOutQuad(0.5f) == doctest::Approx(0.5f));

    CHECK(ease::inCubic(0.5f) == doctest::Approx(0.125f));
    CHECK(ease::outCubic(0.5f) == doctest::Approx(0.875f));
    CHECK(ease::inOutCubic(0.5f) == doctest::Approx(0.5f));

    CHECK(ease::inQuart(0.5f) == doctest::Approx(0.0625f));
    CHECK(ease::outQuart(0.5f) == doctest::Approx(0.9375f));
    CHECK(ease::inOutQuart(0.5f) == doctest::Approx(0.5f));

    CHECK(ease::inQuint(0.5f) == doctest::Approx(0.03125f));
    CHECK(ease::outQuint(0.5f) == doctest::Approx(0.96875f));
    CHECK(ease::inOutQuint(0.5f) == doctest::Approx(0.5f));

    CHECK(ease::inOutSine(0.5f) == doctest::Approx(0.5f));
    CHECK(ease::inOutExpo(0.5f) == doctest::Approx(0.5f));
    CHECK(ease::inOutCirc(0.5f) == doctest::Approx(0.5f));
    CHECK(ease::smoothStep(0.5f) == doctest::Approx(0.5f));
    CHECK(ease::smootherStep(0.5f) == doctest::Approx(0.5f));
}

TEST_CASE("In and Out easing pairs are symmetric mirrors") {
    const float samples[] = {0.1f, 0.25f, 0.4f, 0.7f, 0.9f};
    for (float t : samples) {
        CHECK(ease::inQuad(t) + ease::outQuad(1.0f - t) == doctest::Approx(1.0f));
        CHECK(ease::inCubic(t) + ease::outCubic(1.0f - t) == doctest::Approx(1.0f));
        CHECK(ease::inSine(t) + ease::outSine(1.0f - t) == doctest::Approx(1.0f));
        CHECK(ease::inCirc(t) + ease::outCirc(1.0f - t) == doctest::Approx(1.0f));
        CHECK(ease::inBounce(t) + ease::outBounce(1.0f - t) == doctest::Approx(1.0f));
    }
}

TEST_CASE("Back and Elastic overshoot while Bounce stays within [0, 1]") {
    CHECK(ease::inBack(0.25f) < 0.0f);
    CHECK(ease::outBack(0.75f) > 1.0f);
    CHECK(ease::outElastic(0.2f) > 1.0f);

    for (int i = 0; i <= 50; i++) {
        const float t = static_cast<float>(i) / 50.0f;
        const float b = ease::outBounce(t);
        CHECK(b >= -1e-5f);
        CHECK(b <= 1.0f + 1e-5f);
    }
}
