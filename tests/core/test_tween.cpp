#include <doctest/doctest.h>
#include <mge/core/Tween.h>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cmath>
#include <vector>

using namespace mge;

TEST_CASE("TweenManager interpolates scalar, vector, and quaternion values") {
    TweenManager manager;
    float value = 0.0f;
    glm::vec3 pos{0.0f, 0.0f, 0.0f};
    const glm::quat startRot = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    const glm::quat endRot = glm::angleAxis(glm::half_pi<float>(), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::quat rot = startRot;
    bool done = false;

    TweenOptions opts;
    opts.easing = ease::inQuad;
    opts.onComplete = [&] { done = true; };

    TweenHandle h1 = manager.tweenTo(value, 10.0f, 1.0f, opts);
    TweenHandle h2 = manager.tweenTo(pos, glm::vec3(10.0f, 20.0f, -10.0f), 1.0f);
    TweenHandle h3 = manager.tweenTo(rot, endRot, 1.0f);

    CHECK(h1.isValid());
    CHECK(h2.isValid());
    CHECK(h3.isValid());
    CHECK(manager.activeCount() == 3);

    manager.update(0.5f);
    CHECK(value == doctest::Approx(2.5f)); // inQuad(0.5) = 0.25
    CHECK(pos.x == doctest::Approx(5.0f));
    CHECK(pos.y == doctest::Approx(10.0f));
    CHECK(pos.z == doctest::Approx(-5.0f));
    const glm::quat expectedMid = glm::angleAxis(glm::quarter_pi<float>(), glm::vec3(0.0f, 1.0f, 0.0f));
    CHECK(std::abs(glm::dot(rot, expectedMid)) == doctest::Approx(1.0f).epsilon(1e-4));
    CHECK_FALSE(done);

    manager.update(0.5f);
    CHECK(value == doctest::Approx(10.0f));
    CHECK(pos.x == doctest::Approx(10.0f));
    CHECK(std::abs(glm::dot(rot, endRot)) == doctest::Approx(1.0f).epsilon(1e-4));
    CHECK(done);
    CHECK(manager.activeCount() == 0);
}

TEST_CASE("Tween delay, Loop, and PingPong modes") {
    TweenManager manager;
    float delayed = 0.0f;
    TweenOptions delayOpts;
    delayOpts.delay = 0.5f;
    manager.tweenTo(delayed, 10.0f, 1.0f, delayOpts);

    manager.update(0.3f);
    CHECK(delayed == doctest::Approx(0.0f));
    manager.update(0.7f); // 0.2s left of delay, 0.5s into tween
    CHECK(delayed == doctest::Approx(5.0f));
    manager.update(0.5f);
    CHECK(delayed == doctest::Approx(10.0f));

    // PingPong with 2 cycles: forward (0 -> 10), then backward (10 -> 0)
    float pingPong = 0.0f;
    bool finished = false;
    TweenOptions ppOpts;
    ppOpts.loopMode = LoopMode::PingPong;
    ppOpts.repeatCount = 2;
    ppOpts.onComplete = [&] { finished = true; };
    manager.tweenTo(pingPong, 10.0f, 1.0f, ppOpts);

    manager.update(0.5f);
    CHECK(pingPong == doctest::Approx(5.0f));
    manager.update(0.5f);
    CHECK(pingPong == doctest::Approx(10.0f));
    CHECK_FALSE(finished);
    manager.update(0.5f);
    CHECK(pingPong == doctest::Approx(5.0f));
    manager.update(0.5f);
    CHECK(pingPong == doctest::Approx(0.0f));
    CHECK(finished);
    CHECK(manager.activeCount() == 0);
}

TEST_CASE("TweenSequence chains tweens, intervals, and callbacks") {
    TweenManager manager;
    float x = 0.0f;
    std::vector<int> events;

    TweenSequence seq;
    seq.appendCallback([&] { events.push_back(1); })
        .append(0.0f, 10.0f, 1.0f, [&](const float& v) { x = v; })
        .appendInterval(0.5f)
        .appendCallback([&] { events.push_back(2); })
        .append(10.0f, 20.0f, 1.0f, [&](const float& v) { x = v; })
        .onComplete([&] { events.push_back(3); });

    CHECK(seq.stepCount() == 5);
    manager.startSequence(std::move(seq));

    manager.update(0.5f);
    CHECK(events == std::vector<int>{1});
    CHECK(x == doctest::Approx(5.0f));

    manager.update(0.75f); // finishes first tween (0.5s) + half of interval (0.25s)
    CHECK(x == doctest::Approx(10.0f));
    CHECK(events == std::vector<int>{1});

    manager.update(0.75f); // finishes interval (0.25s) + callback 2 + half of second tween (0.5s)
    CHECK(events == std::vector<int>{1, 2});
    CHECK(x == doctest::Approx(15.0f));

    manager.update(0.5f);
    CHECK(x == doctest::Approx(20.0f));
    CHECK(events == std::vector<int>{1, 2, 3});
    CHECK(manager.activeCount() == 0);
}

TEST_CASE("Tween pause, cancel, zero-duration, and re-entrancy") {
    TweenManager manager;
    float a = 0.0f;
    TweenHandle h = manager.tweenTo(a, 10.0f, 1.0f);
    manager.pause(h);
    CHECK(manager.isPaused(h));
    manager.update(0.5f);
    CHECK(a == doctest::Approx(0.0f));
    manager.resume(h);
    manager.update(0.5f);
    CHECK(a == doctest::Approx(5.0f));
    CHECK(manager.cancel(h));
    manager.update(1.0f);
    CHECK(a == doctest::Approx(5.0f));

    // Zero-duration tween snaps immediately without NaN and can chain another tween in onComplete.
    float b = 0.0f;
    float c = 0.0f;
    TweenOptions opts;
    opts.onComplete = [&] { manager.tweenTo(c, 4.0f, 0.5f); };
    manager.tweenTo(b, 8.0f, 0.0f, opts);
    manager.update(0.016f);
    CHECK(b == doctest::Approx(8.0f));
    CHECK(manager.activeCount() == 1);

    manager.update(0.5f);
    CHECK(c == doctest::Approx(4.0f));
    CHECK(manager.activeCount() == 0);
}
