#include <doctest/doctest.h>
#include <mge/core/math/Smoothing.h>

#include <cmath>

#include <glm/gtc/quaternion.hpp>

using namespace mge;

namespace {
    constexpr float kPi = 3.14159265358979323846f;
    constexpr float kTwoPi = 2.0f * kPi;

    float simulateDamp(float start, float target, float lambda, float totalTime, int steps) {
        const float dt = totalTime / static_cast<float>(steps);
        float value = start;
        for (int i = 0; i < steps; i++) value = damp(value, target, lambda, dt);
        return value;
    }

    glm::vec3 simulateDampVec3(const glm::vec3& start, const glm::vec3& target, float lambda, float totalTime,
                               int steps) {
        const float dt = totalTime / static_cast<float>(steps);
        glm::vec3 value = start;
        for (int i = 0; i < steps; i++) value = damp(value, target, lambda, dt);
        return value;
    }

    glm::quat simulateDampQuat(const glm::quat& start, const glm::quat& target, float lambda, float totalTime,
                               int steps) {
        const float dt = totalTime / static_cast<float>(steps);
        glm::quat value = start;
        for (int i = 0; i < steps; i++) value = damp(value, target, lambda, dt);
        return value;
    }

    float simulateSmoothDamp(float start, float target, float& velocity, float smoothTime, float totalTime,
                             int steps) {
        const float dt = totalTime / static_cast<float>(steps);
        float value = start;
        for (int i = 0; i < steps; i++) value = smoothDamp(value, target, velocity, smoothTime, dt);
        return value;
    }

    glm::vec3 simulateSmoothDampVec3(const glm::vec3& start, const glm::vec3& target, glm::vec3& velocity,
                                     float smoothTime, float totalTime, int steps) {
        const float dt = totalTime / static_cast<float>(steps);
        glm::vec3 value = start;
        for (int i = 0; i < steps; i++) value = smoothDamp(value, target, velocity, smoothTime, dt);
        return value;
    }
}

TEST_CASE("dampFactor: bounds, edge cases, and exact decay") {
    CHECK(dampFactor(5.0f, 0.0f) == 0.0f);
    CHECK(dampFactor(5.0f, -0.1f) == 0.0f);
    CHECK(dampFactor(0.0f, 0.1f) == 0.0f);
    CHECK(dampFactor(-2.0f, 0.1f) == 0.0f);
    CHECK(dampFactor(2.0f, 0.5f) == doctest::Approx(1.0f - std::exp(-1.0f)));
    CHECK(dampFactor(1000.0f, 1.0f) == doctest::Approx(1.0f));
    CHECK(dampFactor(4.0f, 0.1f) < dampFactor(4.0f, 0.2f));
}

TEST_CASE("damp (float): frame-rate independent across 30, 60, 240 FPS and single step") {
    const float expected = 10.0f * (1.0f - std::exp(-2.0f)); // lambda=4, T=0.5
    CHECK(simulateDamp(0.0f, 10.0f, 4.0f, 0.5f, 1) == doctest::Approx(expected).epsilon(1e-4));
    CHECK(simulateDamp(0.0f, 10.0f, 4.0f, 0.5f, 15) == doctest::Approx(expected).epsilon(1e-4));
    CHECK(simulateDamp(0.0f, 10.0f, 4.0f, 0.5f, 30) == doctest::Approx(expected).epsilon(1e-4));
    CHECK(simulateDamp(0.0f, 10.0f, 4.0f, 0.5f, 120) == doctest::Approx(expected).epsilon(1e-4));

    // Paused or zero decay leaves value unchanged
    CHECK(damp(3.0f, 10.0f, 5.0f, 0.0f) == 3.0f);
    CHECK(damp(3.0f, 10.0f, 5.0f, -0.5f) == 3.0f);
    CHECK(damp(3.0f, 10.0f, 0.0f, 0.5f) == 3.0f);
}

TEST_CASE("damp (vec2, vec3, vec4): frame-rate independent and converges") {
    const glm::vec3 start(1.0f, -2.0f, 3.0f);
    const glm::vec3 target(11.0f, 8.0f, -7.0f);
    const glm::vec3 single = simulateDampVec3(start, target, 5.0f, 0.5f, 1);
    const glm::vec3 fps30 = simulateDampVec3(start, target, 5.0f, 0.5f, 15);
    const glm::vec3 fps144 = simulateDampVec3(start, target, 5.0f, 0.5f, 72);

    for (int i = 0; i < 3; i++) {
        CHECK(fps30[i] == doctest::Approx(single[i]).epsilon(1e-4));
        CHECK(fps144[i] == doctest::Approx(single[i]).epsilon(1e-4));
    }

    const glm::vec2 v2 = damp(glm::vec2(0.0f, 10.0f), glm::vec2(10.0f, 0.0f), 10.0f, 1.0f);
    CHECK(v2.x == doctest::Approx(10.0f).epsilon(1e-3));
    CHECK(v2.y == doctest::Approx(0.0f).epsilon(1e-3));

    const glm::vec4 v4 = damp(glm::vec4(0.0f), glm::vec4(1.0f, 2.0f, 3.0f, 4.0f), 10.0f, 1.0f);
    CHECK(v4.x == doctest::Approx(1.0f).epsilon(1e-3));
    CHECK(v4.w == doctest::Approx(4.0f).epsilon(1e-3));
}

TEST_CASE("damp (quat): frame-rate independent, unit length, and shortest arc") {
    const glm::quat identity(1.0f, 0.0f, 0.0f, 0.0f);
    const glm::quat target = glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    const glm::quat single = simulateDampQuat(identity, target, 4.0f, 0.5f, 1);
    const glm::quat fps30 = simulateDampQuat(identity, target, 4.0f, 0.5f, 15);
    const glm::quat fps120 = simulateDampQuat(identity, target, 4.0f, 0.5f, 60);

    CHECK(glm::length(fps120) == doctest::Approx(1.0f));
    CHECK(std::abs(glm::dot(single, fps30)) == doctest::Approx(1.0f).epsilon(1e-4));
    CHECK(std::abs(glm::dot(single, fps120)) == doctest::Approx(1.0f).epsilon(1e-4));

    // Antipodal equivalent quaternion (-target) still takes the shortest arc
    const glm::quat negated = damp(identity, -target, 4.0f, 0.5f);
    CHECK(std::abs(glm::dot(negated, single)) == doctest::Approx(1.0f).epsilon(1e-4));

    // Paused leaves quaternion unchanged
    const glm::quat paused = damp(identity, target, 4.0f, 0.0f);
    CHECK(paused == identity);
}

TEST_CASE("deltaAngle and dampAngle: shortest arc wrapping and frame-rate independence") {
    CHECK(deltaAngle(0.0f, 0.5f) == doctest::Approx(0.5f));
    CHECK(deltaAngle(0.1f, kTwoPi - 0.1f) == doctest::Approx(-0.2f).epsilon(1e-4));
    CHECK(deltaAngle(kTwoPi - 0.1f, 0.1f) == doctest::Approx(0.2f).epsilon(1e-4));
    CHECK(deltaAngle(0.0f, 3.0f * kTwoPi + 0.25f) == doctest::Approx(0.25f).epsilon(1e-4));

    // Moves negatively from 0.1 toward 2*pi - 0.1 (shortest arc across 0)
    const float stepped = dampAngle(0.1f, kTwoPi - 0.1f, 5.0f, 0.1f);
    CHECK(stepped < 0.1f);
    CHECK(stepped > -0.1f);

    float angle30 = 0.1f;
    float angle120 = 0.1f;
    for (int i = 0; i < 15; i++) angle30 = dampAngle(angle30, kTwoPi - 0.1f, 5.0f, 0.5f / 15.0f);
    for (int i = 0; i < 60; i++) angle120 = dampAngle(angle120, kTwoPi - 0.1f, 5.0f, 0.5f / 60.0f);
    CHECK(deltaAngle(angle30, angle120) == doctest::Approx(0.0f).epsilon(1e-4));
}

TEST_CASE("smoothDamp (float): frame-rate independent position and velocity") {
    float velSingle = 0.0f;
    float vel30 = 0.0f;
    float vel60 = 0.0f;
    float vel240 = 0.0f;

    const float posSingle = simulateSmoothDamp(0.0f, 10.0f, velSingle, 0.25f, 0.5f, 1);
    const float pos30 = simulateSmoothDamp(0.0f, 10.0f, vel30, 0.25f, 0.5f, 15);
    const float pos60 = simulateSmoothDamp(0.0f, 10.0f, vel60, 0.25f, 0.5f, 30);
    const float pos240 = simulateSmoothDamp(0.0f, 10.0f, vel240, 0.25f, 0.5f, 120);

    CHECK(pos30 == doctest::Approx(posSingle).epsilon(1e-4));
    CHECK(pos60 == doctest::Approx(posSingle).epsilon(1e-4));
    CHECK(pos240 == doctest::Approx(posSingle).epsilon(1e-4));
    CHECK(vel30 == doctest::Approx(velSingle).epsilon(1e-4));
    CHECK(vel60 == doctest::Approx(velSingle).epsilon(1e-4));
    CHECK(vel240 == doctest::Approx(velSingle).epsilon(1e-4));
}

TEST_CASE("smoothDamp (float): monotonic convergence without overshoot, maxSpeed, and edge cases") {
    // Monotonic approach from rest without overshoot
    float value = 0.0f;
    float velocity = 0.0f;
    for (int i = 0; i < 180; i++) {
        const float next = smoothDamp(value, 10.0f, velocity, 0.2f, 1.0f / 60.0f);
        CHECK(next >= value);
        CHECK(next <= 10.0f);
        value = next;
    }
    CHECK(value == doctest::Approx(10.0f).epsilon(1e-3));
    CHECK(std::abs(velocity) < 1e-2f);

    // maxSpeed caps movement speed
    float capped = 0.0f;
    float cappedVel = 0.0f;
    for (int i = 0; i < 60; i++) {
        const float prev = capped;
        capped = smoothDamp(capped, 1000.0f, cappedVel, 0.1f, 1.0f / 60.0f, 5.0f);
        CHECK((capped - prev) <= 5.0f / 60.0f + 1e-4f);
    }
    CHECK(capped <= 5.0f + 1e-3f);
    CHECK(capped > 3.0f);

    // Overshoot prevention when initial velocity pushes past target
    float fastVel = 100.0f;
    const float clamped = smoothDamp(9.9f, 10.0f, fastVel, 0.2f, 0.1f);
    CHECK(clamped == 10.0f);
    CHECK(fastVel == 0.0f);

    // dt <= 0 leaves position and velocity unchanged
    float pausedVel = 3.5f;
    CHECK(smoothDamp(2.0f, 10.0f, pausedVel, 0.2f, 0.0f) == 2.0f);
    CHECK(pausedVel == 3.5f);
    CHECK(smoothDamp(2.0f, 10.0f, pausedVel, 0.2f, -0.1f) == 2.0f);
    CHECK(pausedVel == 3.5f);

    // smoothTime <= 0 is clamped safely without NaN/Inf
    float snapVel = 0.0f;
    const float snapped = smoothDamp(0.0f, 10.0f, snapVel, 0.0f, 1.0f / 60.0f);
    CHECK(std::isfinite(snapped));
    CHECK(std::isfinite(snapVel));
    CHECK(snapped == doctest::Approx(10.0f).epsilon(1e-3));
}

TEST_CASE("smoothDamp (vec2, vec3, vec4): frame-rate independence, maxSpeed, and overshoot") {
    const glm::vec3 start(0.0f, 5.0f, -3.0f);
    const glm::vec3 target(10.0f, -5.0f, 7.0f);
    glm::vec3 velSingle(0.0f);
    glm::vec3 vel60(0.0f);
    glm::vec3 vel144(0.0f);

    const glm::vec3 posSingle = simulateSmoothDampVec3(start, target, velSingle, 0.3f, 0.5f, 1);
    const glm::vec3 pos60 = simulateSmoothDampVec3(start, target, vel60, 0.3f, 0.5f, 30);
    const glm::vec3 pos144 = simulateSmoothDampVec3(start, target, vel144, 0.3f, 0.5f, 72);

    for (int i = 0; i < 3; i++) {
        CHECK(pos60[i] == doctest::Approx(posSingle[i]).epsilon(1e-4));
        CHECK(pos144[i] == doctest::Approx(posSingle[i]).epsilon(1e-4));
        CHECK(vel60[i] == doctest::Approx(velSingle[i]).epsilon(1e-4));
        CHECK(vel144[i] == doctest::Approx(velSingle[i]).epsilon(1e-4));
    }

    // maxSpeed on vec3
    glm::vec3 capped(0.0f);
    glm::vec3 cappedVel(0.0f);
    for (int i = 0; i < 60; i++) {
        capped = smoothDamp(capped, glm::vec3(1000.0f, 0.0f, 0.0f), cappedVel, 0.1f, 1.0f / 60.0f, 5.0f);
    }
    CHECK(glm::length(capped) <= 5.0f + 1e-3f);

    // Overshoot prevention on vec3
    glm::vec3 fastVel(100.0f, 0.0f, 0.0f);
    const glm::vec3 clamped =
        smoothDamp(glm::vec3(9.9f, 0.0f, 0.0f), glm::vec3(10.0f, 0.0f, 0.0f), fastVel, 0.2f, 0.1f);
    CHECK(clamped == glm::vec3(10.0f, 0.0f, 0.0f));
    CHECK(fastVel == glm::vec3(0.0f));

    // vec2 and vec4 overloads
    glm::vec2 vel2(0.0f);
    glm::vec2 pos2(0.0f);
    for (int i = 0; i < 120; i++) pos2 = smoothDamp(pos2, glm::vec2(4.0f, -6.0f), vel2, 0.15f, 1.0f / 60.0f);
    CHECK(pos2.x == doctest::Approx(4.0f).epsilon(1e-3));
    CHECK(pos2.y == doctest::Approx(-6.0f).epsilon(1e-3));

    glm::vec4 vel4(0.0f);
    glm::vec4 pos4(0.0f);
    for (int i = 0; i < 120; i++) {
        pos4 = smoothDamp(pos4, glm::vec4(1.0f, 2.0f, 3.0f, 4.0f), vel4, 0.15f, 1.0f / 60.0f);
    }
    CHECK(pos4.x == doctest::Approx(1.0f).epsilon(1e-3));
    CHECK(pos4.w == doctest::Approx(4.0f).epsilon(1e-3));
}

TEST_CASE("smoothDampAngle: wraps along shortest arc and converges") {
    float angle = 0.1f;
    float velocity = 0.0f;
    for (int i = 0; i < 120; i++) {
        const float next = smoothDampAngle(angle, kTwoPi - 0.1f, velocity, 0.2f, 1.0f / 60.0f);
        CHECK(next <= angle); // Moves negatively across 0 rather than the long way around
        angle = next;
    }
    CHECK(deltaAngle(angle, kTwoPi - 0.1f) == doctest::Approx(0.0f).epsilon(1e-3));
}
