#include <doctest/doctest.h>
#include <examples/sdf_glide/SdfGlideLogic.h>
#include <mge/core/Random.h>
#include <mge/procgen/Sdf.h>
#include <mge/synth/Sfx.h>

#include <algorithm>
#include <cmath>
#include <numeric>

using namespace sdf_glide;

TEST_CASE("Glide steering accumulates mouse movement until the next fixed step") {
    GlideMouseSteering singleFrame;
    singleFrame.record({6.0f, -8.0f});

    GlideMouseSteering fourFrames;
    for (const auto& delta :
         {glm::vec2(2.0f, -1.0f), glm::vec2(1.0f, -4.0f), glm::vec2(0.0f, 1.0f), glm::vec2(3.0f, -4.0f)}) {
        fourFrames.record(delta);
    }

    const GlideInput keyboard{0.25f, -0.25f};
    auto oneFrame = keyboard;
    auto fourFrame = keyboard;
    singleFrame.apply(oneFrame);
    fourFrames.apply(fourFrame);
    CHECK(oneFrame.steerX == doctest::Approx(0.55f));
    CHECK(oneFrame.steerY == doctest::Approx(0.15f));
    CHECK(fourFrame.steerX == doctest::Approx(oneFrame.steerX));
    CHECK(fourFrame.steerY == doctest::Approx(oneFrame.steerY));

    auto nextTick = keyboard;
    fourFrames.apply(nextTick);
    CHECK(nextTick.steerX == keyboard.steerX);
    CHECK(nextTick.steerY == keyboard.steerY);

    fourFrames.record({60.0f, 60.0f});
    fourFrames.apply(nextTick);
    CHECK(nextTick.steerX == 1.0f);
    CHECK(nextTick.steerY == -1.0f);
}

TEST_CASE("C++ SDF primitives match mge::sdf") {
    // Sphere
    for (float r : {0.5f, 1.0f, 2.5f}) {
        auto mgeSphere = mge::sdf::sphere(r);
        for (const glm::vec3& p :
             {glm::vec3(0.0f), glm::vec3(2.0f, 0.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f)}) {
            CHECK(sdSphere(p, r) == doctest::Approx(mgeSphere(p)));
        }
    }

    // Box
    const glm::vec3 halfExtents(1.0f, 2.0f, 0.5f);
    auto mgeBox = mge::sdf::box(halfExtents);
    for (const glm::vec3& p : {glm::vec3(0.0f), glm::vec3(3.0f, 1.0f, 0.0f), glm::vec3(1.5f, 2.5f, 1.0f)}) {
        CHECK(sdBox(p, halfExtents) == doctest::Approx(mgeBox(p)));
    }

    // Torus
    const float majorRadius = 2.0f;
    const float minorRadius = 0.4f;
    auto mgeTorus = mge::sdf::torus(majorRadius, minorRadius);
    for (const glm::vec3& p :
         {glm::vec3(2.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.5f, 2.0f)}) {
        CHECK(sdTorus(p, majorRadius, minorRadius) == doctest::Approx(mgeTorus(p)));
    }

    // Smooth union
    const auto sphere1 = mge::sdf::sphere(1.0f);
    const auto sphere2 = mge::sdf::opTranslate(mge::sdf::sphere(1.0f), {1.2f, 0.0f, 0.0f});
    const auto smoothU = mge::sdf::opSmoothUnion(sphere1, sphere2, 0.5f);
    for (float x = -2.0f; x <= 3.0f; x += 0.5f) {
        const glm::vec3 p(x, 0.2f, 0.0f);
        const float d1 = sdSphere(p, 1.0f);
        const float d2 = sdSphere(p - glm::vec3(1.2f, 0.0f, 0.0f), 1.0f);
        CHECK(opSmoothUnion(d1, d2, 0.5f) == doctest::Approx(smoothU(p)));
    }

    // Twist
    const float radiansPerUnit = 0.3f;
    const auto twistedBox = mge::sdf::opTwist(mge::sdf::box({1.0f, 1.0f, 1.0f}), radiansPerUnit);
    for (float y = -2.0f; y <= 2.0f; y += 1.0f) {
        const glm::vec3 p(0.8f, y, 0.5f);
        const glm::vec3 tp = opTwist(p, radiansPerUnit);
        CHECK(sdBox(tp, {1.0f, 1.0f, 1.0f}) == doctest::Approx(twistedBox(p)));
    }
}

TEST_CASE("Sampling 1000 points in the procedural canyon (finiteness and Lipschitz continuity)") {
    mge::Pcg32 rng(1337);

    for (int i = 0; i < 1000; ++i) {
        const float x = rng.range(-12.0f, 12.0f);
        const float y = rng.range(-6.0f, 16.0f);
        const float z = rng.range(-500.0f, 500.0f);
        const glm::vec3 p(x, y, z);

        const float d = canyonDistance(p);
        CHECK_FALSE(std::isnan(d));
        CHECK_FALSE(std::isinf(d));

        // Gradient / normal at p
        const glm::vec3 n = canyonNormal(p);
        CHECK_FALSE(std::isnan(n.x));
        CHECK_FALSE(std::isnan(n.y));
        CHECK_FALSE(std::isnan(n.z));

        // Lipschitz condition check with a nearby point
        const glm::vec3 delta(0.05f, -0.04f, 0.03f);
        const float dPerturbed = canyonDistance(p + delta);
        const float diff = std::abs(dPerturbed - d);
        const float maxExpected = glm::length(delta) * 1.5f; // Margin for opTwist and smooth union
        CHECK(diff <= maxExpected);
    }
}

TEST_CASE("sectionHash and sectionHashU32 are deterministic and well-distributed") {
    // Same input produces the exact same result
    CHECK(sectionHashU32(42, 10) == sectionHashU32(42, 10));
    CHECK(sectionHash(42, 10) == sectionHash(42, 10));

    // Range [0, 1)
    for (int s = -10; s <= 50; ++s) {
        for (uint32_t salt = 0; salt < 5; ++salt) {
            const float h = sectionHash(s, salt);
            CHECK(h >= 0.0f);
            CHECK(h < 1.0f);
        }
    }

    // Consecutive sections differ
    for (int s = 0; s < 10; ++s) {
        CHECK(sectionHashU32(s, 1) != sectionHashU32(s + 1, 1));
    }
}

TEST_CASE("GlideGame: initialization and initial state") {
    GlideGame game;
    CHECK_FALSE(game.isGameOver());
    CHECK(game.shields() == 3);
    CHECK(game.energy() == doctest::Approx(100.0f));
    CHECK(game.score() == 0);
    CHECK(game.combo() == 1);
    CHECK(game.distanceTraveled() == doctest::Approx(0.0f));
    CHECK(game.playerPosition().x == doctest::Approx(0.0f));
    CHECK(game.playerPosition().y == doctest::Approx(0.0f));
    CHECK(game.playerPosition().z == doctest::Approx(0.0f));
}

TEST_CASE("GlideGame: forward movement and acceleration") {
    GlideGame game;
    GlideInput input;

    // Advance 1 second
    for (int i = 0; i < 60; ++i) {
        game.update(1.0f / 60.0f, input);
    }

    CHECK(game.distanceTraveled() > 0.0f);
    CHECK(game.playerPosition().z < 0.0f);
}

TEST_CASE("GlideGame: steering with input") {
    GlideGame game;
    GlideInput input;
    input.steerX = 1.0f; // Right
    input.steerY = 1.0f; // Up

    for (int i = 0; i < 20; ++i) {
        game.update(1.0f / 60.0f, input);
    }

    CHECK(game.playerPosition().x > 0.0f);
    CHECK(game.playerPosition().y > 0.0f);
}

TEST_CASE("GlideGame: boost consumes energy and recovers when released") {
    GlideGame game;
    GlideInput input;
    input.boost = true;

    // With boost, energy should decrease and speed should increase
    game.update(0.1f, input);
    CHECK(game.energy() < 100.0f);
    const float boostSpeed = std::abs(game.playerVelocity().z);

    input.boost = false;
    game.update(0.1f, input);
    const float normalSpeed = std::abs(game.playerVelocity().z);
    CHECK(boostSpeed > normalSpeed);

    // Wait for energy to recover
    for (int i = 0; i < 150; ++i) {
        game.update(0.05f, input);
    }
    CHECK(game.energy() == doctest::Approx(100.0f));
}

TEST_CASE("GlideGame: collecting light motes and combos") {
    GlideGame game;
    const auto& motes = game.motes();
    REQUIRE_FALSE(motes.empty());

    // Find the position of the first mote
    const glm::vec3 targetPos = motes[0].position;

    GlideInput input;
    bool collectedAny = false;
    for (int step = 0; step < 240; ++step) {
        // Steer toward the mote's position
        if (game.playerPosition().z > targetPos.z) {
            input.steerX = std::clamp((targetPos.x - game.playerPosition().x) * 2.0f, -1.0f, 1.0f);
            input.steerY = std::clamp((targetPos.y - game.playerPosition().y) * 2.0f, -1.0f, 1.0f);
        }
        game.update(1.0f / 60.0f, input);
        for (auto ev : game.audioEvents()) {
            if (ev == AudioEvent::CollectMote) {
                collectedAny = true;
            }
        }
        if (collectedAny) break;
    }

    CHECK(collectedAny);
    CHECK(game.score() > 0);
    CHECK(game.combo() >= 2);
}

TEST_CASE("GlideGame: canyon collision, bounce, and Game Over after 3 hits") {
    GlideGame game;
    GlideInput input;
    input.steerX = 1.0f; // Continuously collide with the right wall

    int hits = 0;
    for (int step = 0; step < 600; ++step) {
        game.update(1.0f / 60.0f, input);
        for (auto ev : game.audioEvents()) {
            if (ev == AudioEvent::HitWall) {
                hits++;
            }
        }
        if (game.isGameOver()) {
            break;
        }
    }

    CHECK(hits >= 3);
    CHECK(game.shields() == 0);
    CHECK(game.isGameOver());

    // In GameOver, Z position must no longer advance
    const float finalZ = game.playerPosition().z;
    game.update(0.1f, input);
    CHECK(game.playerPosition().z == doctest::Approx(finalZ));

    // Restart via input.restart
    input.restart = true;
    game.update(0.016f, input);
    CHECK_FALSE(game.isGameOver());
    CHECK(game.shields() == 3);
    CHECK(game.score() == 0);
    CHECK(game.distanceTraveled() == doctest::Approx(0.0f));
}

TEST_CASE("SdfGlide procedural audio: valid SFX parameters and background song") {
    auto audioParams = GlideAudioParams::create();

    // 1. Collecting motes with different combos
    for (int i = 0; i < 8; ++i) {
        const auto& sfx = audioParams.moteSounds[i];
        mge::Sample sample = mge::renderSfx(sfx, 44100);
        REQUIRE_FALSE(sample.data.empty());

        float sumSq = 0.0f;
        for (float s : sample.data) {
            CHECK_FALSE(std::isnan(s));
            sumSq += s * s;
        }
        CHECK(sumSq > 0.0f);
    }

    // 2. Wall collision
    {
        mge::Sample sample = mge::renderSfx(audioParams.hitSound, 44100);
        REQUIRE_FALSE(sample.data.empty());
        for (float s : sample.data) CHECK_FALSE(std::isnan(s));
    }

    // 3. Boost
    {
        mge::Sample sample = mge::renderSfx(audioParams.boostSound, 44100);
        REQUIRE_FALSE(sample.data.empty());
        for (float s : sample.data) CHECK_FALSE(std::isnan(s));
    }

    // 4. Theme music
    const mge::Song& song = audioParams.themeSong;
    CHECK(song.valid());
    CHECK(song.bpm() > 0.0f);
    CHECK(song.lengthSteps() > 0);
}

TEST_CASE("GlideGame: pillar collision without tunneling even with dt = 0.25 and boost active") {
    // Find a position at z = p1z + 5.0f in open air (d > kOrbRadius)
    const float h1 = sectionHash(0, 1u);
    const float p1x = -3.8f + (h1 - 0.5f) * 0.6f;
    const float p1z = -7.0f;

    // Find a point in the corridor aimed toward the pillar
    float testX = p1x;
    for (float ox = 0.0f; ox <= 2.0f; ox += 0.2f) {
        if (canyonDistance(glm::vec3(p1x + ox, 0.0f, p1z + 5.0f)) > GlideGame::kOrbRadius + 0.2f) {
            testX = p1x + ox;
            break;
        }
    }

    GlideGame game;
    game.setPlayerPosition(glm::vec3(testX, 0.0f, p1z + 5.0f));
    CHECK(canyonDistance(game.playerPosition()) > GlideGame::kOrbRadius);
    CHECK(game.shields() == 3);

    GlideInput input;
    input.boost = true;
    input.steerX = -1.0f; // Steer toward the left pillar

    game.update(0.25f, input);

    CHECK(game.shields() < 3);

    bool hadHitAudio = false;
    for (auto ev : game.audioEvents()) {
        if (ev == AudioEvent::HitWall) hadHitAudio = true;
    }
    CHECK(hadHitAudio);
}

TEST_CASE("GlideGame: strict determinism across 600 fixed steps") {
    GlideGame game1;
    GlideGame game2;

    constexpr float kFixedDt = 1.0f / 60.0f;
    for (int step = 0; step < 600; ++step) {
        GlideInput input;
        if (step % 50 < 25) {
            input.steerX = 0.4f;
            input.steerY = -0.2f;
        } else {
            input.steerX = -0.3f;
            input.steerY = 0.5f;
        }
        if (step > 100 && step < 200) {
            input.boost = true;
        }

        game1.update(kFixedDt, input);
        game2.update(kFixedDt, input);

        REQUIRE(game1.playerPosition() == game2.playerPosition());
        REQUIRE(game1.previousPosition() == game2.previousPosition());
        REQUIRE(game1.playerVelocity() == game2.playerVelocity());
        REQUIRE(game1.score() == game2.score());
        REQUIRE(game1.shields() == game2.shields());
        REQUIRE(game1.energy() == game2.energy());
        REQUIRE(game1.isGameOver() == game2.isGameOver());
    }
}

TEST_CASE("GlideGame: previousPosition stores the previous position for interpolation") {
    GlideGame game;
    CHECK(game.previousPosition() == game.playerPosition());
    GlideInput input;
    game.update(1.0f / 60.0f, input);
    CHECK(game.previousPosition() != game.playerPosition());
    CHECK(game.previousPosition().z == doctest::Approx(0.0f));
    CHECK(game.playerPosition().z < 0.0f);
}
