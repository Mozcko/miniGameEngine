#include <doctest/doctest.h>
#include <mge/core/Time.h>
#include <mge/core/ecs/Transform.h>
#include <mge/core/ecs/World.h>
#include <mge/core/ecs/Camera.h>
#include <mge/physics/CharacterController.h>
#include <mge/physics/GridQuery.h>

#include <algorithm>

using namespace mge;

TEST_CASE("character: standing still in Walk mode stays grounded without sinking or jittering") {
    // Floor at y = 0 (blocks with y <= 0 are solid, top face at y = 1.0)
    auto isSolid = [](const glm::ivec3& p) { return p.y <= 0; };

    constexpr float kSkin = 1e-4f;
    CharacterController controller;
    glm::vec3 feet(0.0f, 1.0f + kSkin, 0.0f);
    controller.grounded = true;
    controller.previousPosition = feet;

    constexpr float kDt = 1.0f / 60.0f;
    for (int i = 0; i < 600; ++i) {
        feet = stepCharacter(controller, feet, kDt, isSolid);
    }

    CHECK(controller.grounded);
    CHECK(feet.x == doctest::Approx(0.0f));
    CHECK(feet.y == doctest::Approx(1.0f + kSkin));
    CHECK(feet.z == doctest::Approx(0.0f));
    CHECK(controller.velocity.x == doctest::Approx(0.0f));
    CHECK(controller.velocity.y == doctest::Approx(0.0f));
    CHECK(controller.velocity.z == doctest::Approx(0.0f));
}

TEST_CASE("character: falls from y = 10, lands, becomes grounded with velocity.y == 0") {
    auto isSolid = [](const glm::ivec3& p) { return p.y <= 0; };

    constexpr float kSkin = 1e-4f;
    CharacterController controller;
    glm::vec3 feet(0.0f, 10.0f, 0.0f);
    controller.grounded = false;

    constexpr float kDt = 1.0f / 60.0f;
    for (int i = 0; i < 120; ++i) {
        feet = stepCharacter(controller, feet, kDt, isSolid);
        if (controller.grounded) {
            break;
        }
    }

    CHECK(controller.grounded);
    CHECK(feet.y == doctest::Approx(1.0f + kSkin));
    CHECK(controller.velocity.y == doctest::Approx(0.0f));
}

TEST_CASE("character: jump reaches expected apex height and lands back down") {
    auto isSolid = [](const glm::ivec3& p) { return p.y <= 0; };

    constexpr float kSkin = 1e-4f;
    CharacterController controller;
    glm::vec3 feet(0.0f, 1.0f + kSkin, 0.0f);
    controller.grounded = true;

    // Expected apex height from kinematics: v^2 / (2 * g)
    const float expectedApexDelta =
        (controller.jumpSpeed * controller.jumpSpeed) / (2.0f * controller.gravity);

    constexpr float kDt = 1.0f / 60.0f;
    controller.input.jump = true;
    feet = stepCharacter(controller, feet, kDt, isSolid);
    controller.input.jump = false;

    float maxHeight = feet.y;
    for (int i = 0; i < 180; ++i) {
        feet = stepCharacter(controller, feet, kDt, isSolid);
        maxHeight = std::max(maxHeight, feet.y);
        if (controller.grounded) {
            break;
        }
    }

    const float actualApexDelta = maxHeight - (1.0f + kSkin);
    CHECK(actualApexDelta == doctest::Approx(expectedApexDelta).epsilon(0.1f));
    CHECK(controller.grounded);
    CHECK(feet.y == doctest::Approx(1.0f + kSkin));
    CHECK(controller.velocity.y == doctest::Approx(0.0f));
}

TEST_CASE("character: walks forward at walkSpeed along flatForward ignoring pitch") {
    auto isSolid = [](const glm::ivec3& p) { return p.y <= 0; };

    constexpr float kSkin = 1e-4f;
    CharacterController controller;
    glm::vec3 feet(0.0f, 1.0f + kSkin, 0.0f);
    controller.grounded = true;

    // Look down 45 degrees (pitch = -45) with yaw = -90 (facing -Z)
    controller.look(0.0f, -45.0f);
    controller.input.moveForward = 1.0f;

    constexpr float kDt = 1.0f / 60.0f;
    // With 40 m/s^2 acceleration, reaches 4.3 m/s in ~0.11 s (7 frames)
    for (int i = 0; i < 60; ++i) {
        feet = stepCharacter(controller, feet, kDt, isSolid);
    }

    CHECK(controller.velocity.z == doctest::Approx(-controller.walkSpeed).epsilon(0.01f));
    CHECK(controller.velocity.x == doctest::Approx(0.0f).epsilon(0.01f));
    CHECK(controller.velocity.y == doctest::Approx(0.0f).epsilon(0.01f));
    CHECK(feet.z < -1.0f);
    CHECK(feet.x == doctest::Approx(0.0f).epsilon(0.01f));
    CHECK(feet.y == doctest::Approx(1.0f + kSkin));
}

TEST_CASE("character: step-up climbs 1-block step with stepHeight 1.0 but not with 0.6") {
    // Floor at y = 0 for z > 0; 1-block step (y = 1) for z <= 0
    auto isSolid = [](const glm::ivec3& p) { return p.y <= 0 || (p.y == 1 && p.z <= 0); };

    constexpr float kSkin = 1e-4f;
    constexpr float kDt = 1.0f / 60.0f;

    // Case A: with stepHeight = 1.05f it can climb the 1.0-unit step
    {
        CharacterController controller;
        controller.stepHeight = 1.05f;
        glm::vec3 feet(0.0f, 1.0f + kSkin, 1.0f);
        controller.grounded = true;
        controller.input.moveForward = 1.0f;

        for (int i = 0; i < 60; ++i) {
            feet = stepCharacter(controller, feet, kDt, isSolid);
        }

        // Should have stepped onto the top of block y=1 (top face at y=2.0)
        CHECK(feet.y == doctest::Approx(2.0f + kSkin));
        CHECK(feet.z < 0.0f);
        CHECK(controller.grounded);
    }

    // Case B: with stepHeight = 0.6f (default) it cannot climb the 1.0-unit step
    {
        CharacterController controller;
        controller.stepHeight = 0.6f;
        glm::vec3 feet(0.0f, 1.0f + kSkin, 1.0f);
        controller.grounded = true;
        controller.input.moveForward = 1.0f;

        for (int i = 0; i < 60; ++i) {
            feet = stepCharacter(controller, feet, kDt, isSolid);
        }

        // Remains blocked in front of the step on the original floor
        CHECK(feet.y == doctest::Approx(1.0f + kSkin));
        CHECK(feet.z > 0.0f);
    }
}

TEST_CASE("character: Fly mode has no gravity, moveUp ascends and stops on collision") {
    constexpr float kSkin = 1e-4f;
    constexpr float kDt = 1.0f / 60.0f;

    // Ceiling at y = 15
    auto isSolid = [](const glm::ivec3& p) { return p.y >= 15; };

    CharacterController controller;
    controller.setMode(CharacterMode::Fly);
    glm::vec3 feet(0.0f, 10.0f, 0.0f);

    // 1. Without input in mid-air it does not fall
    for (int i = 0; i < 60; ++i) {
        feet = stepCharacter(controller, feet, kDt, isSolid);
    }
    CHECK(feet.y == doctest::Approx(10.0f));
    CHECK(controller.velocity == glm::vec3(0.0f));

    // 2. moveUp accelerates vertically upward
    controller.input.moveUp = 1.0f;
    for (int i = 0; i < 120; ++i) {
        feet = stepCharacter(controller, feet, kDt, isSolid);
    }

    // Maximum height limited by ceiling at y = 15
    // Box height is 2 * halfExtents.y (1.8m), top touches 15.0 - skin
    const float expectedMaxFeetY = 15.0f - kSkin - 2.0f * controller.halfExtents.y;
    CHECK(feet.y == doctest::Approx(expectedMaxFeetY));
    CHECK(controller.velocity.y == doctest::Approx(0.0f));
}

TEST_CASE("character: toggleMode, enabled = false, and dt = 0") {
    auto isSolid = [](const glm::ivec3&) { return false; };

    CharacterController controller;
    controller.velocity = glm::vec3(0.0f, -10.0f, 0.0f);

    // When toggling to Fly, velocity.y resets to 0
    controller.toggleMode();
    CHECK(controller.mode == CharacterMode::Fly);
    CHECK(controller.velocity.y == doctest::Approx(0.0f));

    controller.toggleMode();
    CHECK(controller.mode == CharacterMode::Walk);

    // enabled = false does not move the entity
    controller.enabled = false;
    controller.input.moveForward = 1.0f;
    const glm::vec3 originalFeet(1.0f, 2.0f, 3.0f);
    glm::vec3 feet = stepCharacter(controller, originalFeet, 0.1f, isSolid);
    CHECK(feet == originalFeet);

    // dt = 0 does not move the entity
    controller.enabled = true;
    feet = stepCharacter(controller, originalFeet, 0.0f, isSolid);
    CHECK(feet == originalFeet);
}

TEST_CASE("character: determinism across two 300-step runs") {
    auto isSolid = [](const glm::ivec3& p) { return p.y <= 0 || (p.y == 1 && p.z <= -2); };

    auto simulateRun = [&](glm::vec3& outFeet, CharacterController& outController) {
        CharacterController ctrl;
        ctrl.stepHeight = 1.05f;
        glm::vec3 feet(0.0f, 5.0f, 0.0f);
        constexpr float kDt = 1.0f / 60.0f;

        for (int i = 0; i < 300; ++i) {
            ctrl.input.moveForward = (i > 50) ? 1.0f : 0.0f;
            ctrl.input.jump = (i >= 80 && i <= 85);
            ctrl.input.sprint = (i > 120);
            if (i == 150) {
                ctrl.look(15.0f, 5.0f);
            }
            feet = stepCharacter(ctrl, feet, kDt, isSolid);
        }
        outFeet = feet;
        outController = ctrl;
    };

    glm::vec3 feetA{0.0f};
    CharacterController ctrlA;
    simulateRun(feetA, ctrlA);

    glm::vec3 feetB{0.0f};
    CharacterController ctrlB;
    simulateRun(feetB, ctrlB);

    CHECK(feetA.x == feetB.x);
    CHECK(feetA.y == feetB.y);
    CHECK(feetA.z == feetB.z);
    CHECK(ctrlA.velocity.x == ctrlB.velocity.x);
    CHECK(ctrlA.velocity.y == ctrlB.velocity.y);
    CHECK(ctrlA.velocity.z == ctrlB.velocity.z);
    CHECK(ctrlA.grounded == ctrlB.grounded);
}

TEST_CASE("character: ECS systems addCharacterSystem and addCharacterCameraSystem") {
    World world;
    auto player = world.registry().create();
    world.registry().add<Transform>(player, glm::vec3(0.0f, 10.0f, 0.0f));
    CharacterController ctrl;
    world.registry().add<CharacterController>(player, ctrl);

    // Physics system in Fixed phase
    addCharacterSystem(world, [](const glm::ivec3&) { return false; });
    world.run(SystemPhase::Fixed, 1.0f / 60.0f);

    const auto& playerTransform = world.registry().get<Transform>(player);
    // Should have descended due to gravity
    CHECK(playerTransform.position.y < 10.0f);

    // Camera linked to player
    auto camera = world.registry().create();
    world.registry().add<Transform>(camera);
    world.registry().add<Camera>(camera);
    world.registry().add<CharacterCamera>(camera, CharacterCamera{player});

    Time time;
    addCharacterCameraSystem(world, time);
    world.run(SystemPhase::Render, 1.0f / 60.0f);

    const auto& cameraTransform = world.registry().get<Transform>(camera);
    const auto& currentCtrl = world.registry().get<CharacterController>(player);
    // With default alpha = 0.0f in Time, position is previousPosition + eyeHeight
    CHECK(cameraTransform.position.y ==
          doctest::Approx(currentCtrl.previousPosition.y + currentCtrl.eyeHeight));

    // Pure interpolation test for characterEyePosition
    Transform trFinal{glm::vec3(10.0f, 20.0f, 30.0f)};
    CharacterController ctrlInterpolation;
    ctrlInterpolation.previousPosition = glm::vec3(0.0f, 10.0f, 20.0f);
    ctrlInterpolation.eyeHeight = 1.62f;

    const glm::vec3 eyeHalf = characterEyePosition(ctrlInterpolation, trFinal, 0.5f);
    CHECK(eyeHalf.x == doctest::Approx(5.0f));
    CHECK(eyeHalf.y == doctest::Approx(15.0f + 1.62f));
    CHECK(eyeHalf.z == doctest::Approx(25.0f));
}

TEST_CASE("character: disabled controller updates previousPosition when Transform is moved manually") {
    World world;
    auto player = world.registry().create();
    world.registry().add<Transform>(player, glm::vec3(0.0f, 0.0f, 0.0f));
    CharacterController ctrl;
    ctrl.enabled = false;
    ctrl.previousPosition = glm::vec3(0.0f, 0.0f, 0.0f);
    world.registry().add<CharacterController>(player, ctrl);

    addCharacterSystem(world, [](const glm::ivec3&) { return false; });

    // Teleport / manually move transform while disabled
    const glm::vec3 teleportPos(15.0f, 25.0f, 35.0f);
    world.registry().get<Transform>(player).position = teleportPos;

    constexpr float kDt = 1.0f / 60.0f;
    world.run(SystemPhase::Fixed, kDt);

    const auto& updatedCtrl = world.registry().get<CharacterController>(player);
    const auto& updatedTransform = world.registry().get<Transform>(player);

    CHECK(updatedTransform.position == teleportPos);
    CHECK(updatedCtrl.previousPosition == teleportPos);

    // Also testing stepCharacter directly
    CharacterController directCtrl;
    directCtrl.enabled = false;
    directCtrl.previousPosition = glm::vec3(0.0f);
    const glm::vec3 newFeet(100.0f, 200.0f, 300.0f);
    const glm::vec3 steppedFeet =
        stepCharacter(directCtrl, newFeet, kDt, [](const glm::ivec3&) { return false; });

    CHECK(steppedFeet == newFeet);
    CHECK(directCtrl.previousPosition == newFeet);
}
