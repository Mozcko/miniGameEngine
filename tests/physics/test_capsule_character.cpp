#include <doctest/doctest.h>
#include <mge/core/Time.h>
#include <mge/core/ecs/Camera.h>
#include <mge/core/ecs/Transform.h>
#include <mge/core/ecs/World.h>
#include <mge/physics/CapsuleCharacterController.h>
#include <mge/procgen/MeshData.h>

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

using namespace mge;

namespace {

    // Adds a quad (two CCW triangles) to a MeshCollider.
    void addQuad(MeshCollider& collider, const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2,
                 const glm::vec3& p3) {
        collider.addTriangle(p0, p1, p2);
        collider.addTriangle(p0, p2, p3);
    }

    // Builds a horizontal floor quad at y = floorY spanning [-halfSize, +halfSize] on X and Z.
    MeshCollider makeFlatFloor(float floorY = 0.0f, float halfSize = 20.0f) {
        MeshCollider collider;
        addQuad(collider, {-halfSize, floorY, -halfSize}, {-halfSize, floorY, halfSize},
                {halfSize, floorY, halfSize}, {halfSize, floorY, -halfSize});
        collider.build();
        return collider;
    }

    // Builds a floor at y = 0 for x <= 0 connected to a ramp rising in +X at `angleDeg` degrees.
    MeshCollider makeRampWorld(float angleDeg, float rampRun = 10.0f, float halfWidth = 6.0f) {
        MeshCollider collider;
        const float slope = std::tan(glm::radians(angleDeg));
        const float topY = rampRun * slope;

        // Flat approach floor: x in [-10, 0], y = 0
        addQuad(collider, {-10.0f, 0.0f, -halfWidth}, {-10.0f, 0.0f, halfWidth}, {0.0f, 0.0f, halfWidth},
                {0.0f, 0.0f, -halfWidth});

        // Ramp: x in [0, rampRun], y = x * slope
        addQuad(collider, {0.0f, 0.0f, -halfWidth}, {0.0f, 0.0f, halfWidth}, {rampRun, topY, halfWidth},
                {rampRun, topY, -halfWidth});

        // Top landing: x in [rampRun, rampRun + 6], y = topY
        addQuad(collider, {rampRun, topY, -halfWidth}, {rampRun, topY, halfWidth},
                {rampRun + 6.0f, topY, halfWidth}, {rampRun + 6.0f, topY, -halfWidth});

        collider.build();
        return collider;
    }

    // Builds a floor at y = 0 with a single step at x = stepX of height `stepY`.
    MeshCollider makeStepWorld(float stepX, float stepY, float halfWidth = 6.0f) {
        MeshCollider collider;
        // Lower floor: x in [-10, stepX], y = 0
        addQuad(collider, {-10.0f, 0.0f, -halfWidth}, {-10.0f, 0.0f, halfWidth}, {stepX, 0.0f, halfWidth},
                {stepX, 0.0f, -halfWidth});
        // Vertical riser at x = stepX facing -X
        addQuad(collider, {stepX, 0.0f, -halfWidth}, {stepX, 0.0f, halfWidth}, {stepX, stepY, halfWidth},
                {stepX, stepY, -halfWidth});
        // Upper tread: x in [stepX, stepX + 10], y = stepY
        addQuad(collider, {stepX, stepY, -halfWidth}, {stepX, stepY, halfWidth},
                {stepX + 10.0f, stepY, halfWidth}, {stepX + 10.0f, stepY, -halfWidth});
        collider.build();
        return collider;
    }

} // namespace

TEST_CASE("capsule: MeshCollider builds BVH from MeshData and queries AABBs accurately") {
    MeshData mesh;
    const std::uint32_t v0 = mesh.addVertex({-1.0f, 0.0f, -1.0f});
    const std::uint32_t v1 = mesh.addVertex({-1.0f, 0.0f, 1.0f});
    const std::uint32_t v2 = mesh.addVertex({1.0f, 0.0f, 1.0f});
    const std::uint32_t v3 = mesh.addVertex({1.0f, 0.0f, -1.0f});
    mesh.addQuad(v0, v1, v2, v3);
    // Degenerate triangle should be skipped
    mesh.addTriangle(v0, v0, v1);

    const glm::mat4 xform = glm::translate(glm::mat4(1.0f), glm::vec3(10.0f, 2.0f, 0.0f));
    const MeshCollider collider = MeshCollider::fromMesh(mesh, xform);

    CHECK(collider.triangleCount() == 2);
    CHECK(collider.bounds().min.x == doctest::Approx(9.0f));
    CHECK(collider.bounds().max.x == doctest::Approx(11.0f));
    CHECK(collider.bounds().min.y == doctest::Approx(2.0f));

    std::vector<std::uint32_t> hits;
    collider.queryAabb(Aabb({9.5f, 1.5f, -0.5f}, {10.5f, 2.5f, 0.5f}), hits);
    CHECK(hits.size() == 2);

    collider.queryAabb(Aabb({-5.0f, -1.0f, -1.0f}, {-4.0f, 1.0f, 1.0f}), hits);
    CHECK(hits.empty());
}

TEST_CASE("capsule: standing still on flat mesh floor stays grounded without sinking or jittering") {
    const MeshCollider floor = makeFlatFloor(0.0f);

    CapsuleCharacterController controller;
    glm::vec3 feet(0.0f, controller.skin, 0.0f);
    controller.grounded = true;
    controller.previousPosition = feet;

    constexpr float kDt = 1.0f / 60.0f;
    for (int i = 0; i < 300; ++i) {
        feet = stepCapsuleCharacter(controller, feet, kDt, floor);
    }

    CHECK(controller.grounded);
    CHECK(feet.x == doctest::Approx(0.0f).epsilon(1e-3f));
    CHECK(feet.y == doctest::Approx(controller.skin).epsilon(2e-3f));
    CHECK(feet.z == doctest::Approx(0.0f).epsilon(1e-3f));
    CHECK(controller.velocity.y == doctest::Approx(0.0f));
}

TEST_CASE("capsule: falls onto mesh floor, lands grounded with velocity.y == 0, and does not tunnel") {
    const MeshCollider floor = makeFlatFloor(0.0f);

    SUBCASE("Standard fall under gravity") {
        CapsuleCharacterController controller;
        glm::vec3 feet(0.0f, 8.0f, 0.0f);
        controller.grounded = false;

        constexpr float kDt = 1.0f / 60.0f;
        for (int i = 0; i < 120; ++i) {
            feet = stepCapsuleCharacter(controller, feet, kDt, floor);
            if (controller.grounded) {
                break;
            }
        }

        CHECK(controller.grounded);
        CHECK(feet.y == doctest::Approx(controller.skin).epsilon(2e-3f));
        CHECK(controller.velocity.y == doctest::Approx(0.0f));
        CHECK(controller.groundNormal.y == doctest::Approx(1.0f).epsilon(1e-3f));
    }

    SUBCASE("Huge downward delta does not tunnel through thin triangle floor") {
        const Capsule cap{{0.0f, 5.0f, 0.0f}, 0.35f, 1.8f};
        CapsuleMoveOptions opts;
        opts.skin = 1e-3f;
        const CapsuleMoveResult res = moveCapsule(cap, {0.0f, -50.0f, 0.0f}, floor, opts);

        CHECK(res.grounded);
        CHECK(res.feet.y == doctest::Approx(opts.skin).epsilon(2e-3f));
    }
}

TEST_CASE("capsule: walkable 30-degree slope supports standing still and smooth uphill walking") {
    const MeshCollider world = makeRampWorld(30.0f);
    constexpr float kDt = 1.0f / 60.0f;

    SUBCASE("Standing still on 30-degree slope does not slide downhill") {
        CapsuleCharacterController controller;
        controller.maxSlopeAngle = 45.0f;
        const float slope = std::tan(glm::radians(30.0f));
        // Place capsule slightly above the ramp at x = 3.0 and let it settle
        glm::vec3 feet(3.0f, 3.0f * slope + 0.05f, 0.0f);
        for (int i = 0; i < 30; ++i) {
            feet = stepCapsuleCharacter(controller, feet, kDt, world);
        }
        CHECK(controller.grounded);
        const glm::vec3 settledFeet = feet;

        // Stand still for 240 frames (4 seconds)
        for (int i = 0; i < 240; ++i) {
            feet = stepCapsuleCharacter(controller, feet, kDt, world);
        }

        CHECK(controller.grounded);
        CHECK(feet.x == doctest::Approx(settledFeet.x).epsilon(1e-2f));
        CHECK(feet.y == doctest::Approx(settledFeet.y).epsilon(1e-2f));
        CHECK(controller.groundNormal.y == doctest::Approx(std::cos(glm::radians(30.0f))).epsilon(0.03f));
    }

    SUBCASE("Walking forward climbs 30-degree ramp smoothly") {
        CapsuleCharacterController controller;
        controller.maxSlopeAngle = 45.0f;
        controller.yaw = 0.0f; // Facing +X
        glm::vec3 feet(-2.0f, controller.skin, 0.0f);
        controller.grounded = true;
        controller.input.moveForward = 1.0f;

        for (int i = 0; i < 120; ++i) {
            feet = stepCapsuleCharacter(controller, feet, kDt, world);
        }

        CHECK(controller.grounded);
        CHECK(feet.x > 3.0f);
        const float expectedY = feet.x * std::tan(glm::radians(30.0f));
        CHECK(feet.y == doctest::Approx(expectedY).epsilon(0.15f));
    }
}

TEST_CASE("capsule: steep 60-degree slope blocks uphill walking and slides when landed on") {
    const MeshCollider world = makeRampWorld(60.0f);
    constexpr float kDt = 1.0f / 60.0f;

    SUBCASE("Cannot climb 60-degree slope with maxSlopeAngle 45") {
        CapsuleCharacterController controller;
        controller.maxSlopeAngle = 45.0f;
        controller.yaw = 0.0f; // Facing +X
        glm::vec3 feet(-2.0f, controller.skin, 0.0f);
        controller.grounded = true;
        controller.input.moveForward = 1.0f;

        for (int i = 0; i < 120; ++i) {
            feet = stepCapsuleCharacter(controller, feet, kDt, world);
        }

        // Must remain blocked near the base of the 60-degree ramp
        CHECK(feet.x < 0.25f);
        CHECK(feet.y < 0.3f);
    }

    SUBCASE("Falling onto 60-degree slope slides downhill and is not grounded") {
        CapsuleCharacterController controller;
        controller.maxSlopeAngle = 45.0f;
        const float startX = 3.0f;
        const float rampY = startX * std::tan(glm::radians(60.0f));
        glm::vec3 feet(startX, rampY + 0.5f, 0.0f);
        controller.grounded = false;

        bool sawSteepSlide = false;
        for (int i = 0; i < 60; ++i) {
            feet = stepCapsuleCharacter(controller, feet, kDt, world);
            if (controller.onSteepSlope && !controller.grounded) {
                sawSteepSlide = true;
            }
        }

        CHECK(sawSteepSlide);
        CHECK(feet.x < startX - 0.5f);
        CHECK(feet.y < rampY);
    }
}

TEST_CASE("capsule: step-up climbs steps within stepHeight and rejects taller ledges or low ceilings") {
    constexpr float kDt = 1.0f / 60.0f;

    SUBCASE("Climbs 0.35m step with stepHeight 0.5m") {
        const MeshCollider world = makeStepWorld(1.0f, 0.35f);
        CapsuleCharacterController controller;
        controller.stepHeight = 0.5f;
        controller.yaw = 0.0f; // Facing +X
        glm::vec3 feet(0.0f, controller.skin, 0.0f);
        controller.grounded = true;
        controller.input.moveForward = 1.0f;

        bool didStep = false;
        for (int i = 0; i < 60; ++i) {
            feet = stepCapsuleCharacter(controller, feet, kDt, world);
            if (controller.stepped) {
                didStep = true;
            }
        }

        CHECK(didStep);
        CHECK(controller.grounded);
        CHECK(feet.x > 1.5f);
        CHECK(feet.y == doctest::Approx(0.35f + controller.skin).epsilon(0.01f));
    }

    SUBCASE("Cannot climb 0.8m ledge with stepHeight 0.5m") {
        const MeshCollider world = makeStepWorld(1.0f, 0.8f);
        CapsuleCharacterController controller;
        controller.stepHeight = 0.5f;
        controller.yaw = 0.0f; // Facing +X
        glm::vec3 feet(0.0f, controller.skin, 0.0f);
        controller.grounded = true;
        controller.input.moveForward = 1.0f;

        for (int i = 0; i < 60; ++i) {
            feet = stepCapsuleCharacter(controller, feet, kDt, world);
        }

        CHECK(feet.x < 1.0f - controller.radius + 0.05f);
        CHECK(feet.y == doctest::Approx(controller.skin).epsilon(0.02f));
    }

    SUBCASE("Low ceiling prevents stepping up") {
        MeshCollider world = makeStepWorld(1.0f, 0.35f);
        // Low ceiling at y = 1.95m (capsule height is 1.8m, leaving only 0.15m clearance < 0.35m step)
        addQuad(world, {-10.0f, 1.95f, -6.0f}, {10.0f, 1.95f, -6.0f}, {10.0f, 1.95f, 6.0f},
                {-10.0f, 1.95f, 6.0f});
        world.build();

        CapsuleCharacterController controller;
        controller.stepHeight = 0.5f;
        controller.yaw = 0.0f;
        glm::vec3 feet(0.0f, controller.skin, 0.0f);
        controller.grounded = true;
        controller.input.moveForward = 1.0f;

        for (int i = 0; i < 60; ++i) {
            feet = stepCapsuleCharacter(controller, feet, kDt, world);
        }

        CHECK(feet.x < 1.0f);
        CHECK(feet.y < 0.25f);
    }
}

TEST_CASE("capsule: ground snapping sticks to downhill slopes, skips on jump, and releases over cliffs") {
    constexpr float kDt = 1.0f / 60.0f;

    SUBCASE("Walking down a 30-degree ramp stays grounded on every frame via ground snapping") {
        const MeshCollider world = makeRampWorld(30.0f);
        CapsuleCharacterController controller;
        controller.maxSlopeAngle = 45.0f;
        controller.groundSnapDistance = 0.35f;
        controller.yaw = 180.0f; // Facing -X (downhill)

        const float startX = 8.0f;
        const float startY = startX * std::tan(glm::radians(30.0f));
        glm::vec3 feet(startX, startY + 0.05f, 0.0f);
        for (int i = 0; i < 20; ++i) {
            feet = stepCapsuleCharacter(controller, feet, kDt, world);
        }
        REQUIRE(controller.grounded);

        controller.input.moveForward = 1.0f;
        bool stayedGrounded = true;
        bool sawSnap = false;
        for (int i = 0; i < 90; ++i) {
            feet = stepCapsuleCharacter(controller, feet, kDt, world);
            if (!controller.grounded) {
                stayedGrounded = false;
            }
            if (controller.snapped) {
                sawSnap = true;
            }
        }

        CHECK(stayedGrounded);
        CHECK(sawSnap);
        CHECK(feet.x < startX - 3.0f);
    }

    SUBCASE("Walking off a cliff exceeding groundSnapDistance becomes airborne and falls") {
        MeshCollider world;
        // Elevated platform at y = 5.0 for x <= 2.0, lower floor at y = 0.0
        addQuad(world, {-10.0f, 5.0f, -6.0f}, {-10.0f, 5.0f, 6.0f}, {2.0f, 5.0f, 6.0f},
                {2.0f, 5.0f, -6.0f});
        addQuad(world, {-10.0f, 0.0f, -6.0f}, {-10.0f, 0.0f, 6.0f}, {15.0f, 0.0f, 6.0f},
                {15.0f, 0.0f, -6.0f});
        world.build();

        CapsuleCharacterController controller;
        controller.groundSnapDistance = 0.35f;
        controller.yaw = 0.0f; // Facing +X
        glm::vec3 feet(1.0f, 5.0f + controller.skin, 0.0f);
        controller.grounded = true;
        controller.input.moveForward = 1.0f;

        bool becameAirborne = false;
        for (int i = 0; i < 120; ++i) {
            feet = stepCapsuleCharacter(controller, feet, kDt, world);
            if (!controller.grounded) {
                becameAirborne = true;
            }
        }

        CHECK(becameAirborne);
        CHECK(controller.grounded);
        CHECK(feet.y == doctest::Approx(controller.skin).epsilon(0.01f));
    }

    SUBCASE("Jumping skips ground snapping, reaches expected apex, and lands back down") {
        const MeshCollider floor = makeFlatFloor(0.0f);
        CapsuleCharacterController controller;
        glm::vec3 feet(0.0f, controller.skin, 0.0f);
        controller.grounded = true;

        const float expectedApex =
            (controller.jumpSpeed * controller.jumpSpeed) / (2.0f * controller.gravity);

        controller.input.jump = true;
        feet = stepCapsuleCharacter(controller, feet, kDt, floor);
        controller.input.jump = false;
        CHECK_FALSE(controller.grounded);
        CHECK_FALSE(controller.snapped);

        float maxHeight = feet.y;
        for (int i = 0; i < 180; ++i) {
            feet = stepCapsuleCharacter(controller, feet, kDt, floor);
            maxHeight = std::max(maxHeight, feet.y);
            if (controller.grounded) {
                break;
            }
        }

        CHECK(maxHeight - controller.skin == doctest::Approx(expectedApex).epsilon(0.1f));
        CHECK(controller.grounded);
        CHECK(feet.y == doctest::Approx(controller.skin).epsilon(0.01f));
    }
}

TEST_CASE("capsule: wall sliding and 90-degree corner containment") {
    MeshCollider world;
    // Floor at y = 0
    addQuad(world, {-10.0f, 0.0f, -10.0f}, {-10.0f, 0.0f, 10.0f}, {10.0f, 0.0f, 10.0f},
            {10.0f, 0.0f, -10.0f});
    // Wall along x = 2 (facing -X)
    addQuad(world, {2.0f, 0.0f, -10.0f}, {2.0f, 0.0f, 10.0f}, {2.0f, 5.0f, 10.0f}, {2.0f, 5.0f, -10.0f});
    // Wall along z = 2 (facing -Z)
    addQuad(world, {-10.0f, 0.0f, 2.0f}, {10.0f, 0.0f, 2.0f}, {10.0f, 5.0f, 2.0f}, {-10.0f, 5.0f, 2.0f});
    world.build();

    SUBCASE("Diagonal motion slides smoothly along the x = 2 wall") {
        const Capsule cap{{1.5f, 0.001f, -5.0f}, 0.35f, 1.8f};
        CapsuleMoveOptions opts;
        opts.wasGrounded = true;
        opts.allowStepping = false;
        const CapsuleMoveResult res = moveCapsule(cap, {1.0f, 0.0f, 1.0f}, world, opts);

        CHECK(res.hitWall);
        CHECK(res.feet.x <= 2.0f - cap.radius + 0.01f);
        CHECK(res.feet.z > -4.2f);
    }

    SUBCASE("Pushing diagonally into the (2, 2) corner never penetrates either wall") {
        CapsuleCharacterController controller;
        controller.yaw = 45.0f; // Facing (+X, +Z)
        glm::vec3 feet(1.0f, controller.skin, 1.0f);
        controller.grounded = true;
        controller.input.moveForward = 1.0f;

        constexpr float kDt = 1.0f / 60.0f;
        for (int i = 0; i < 60; ++i) {
            feet = stepCapsuleCharacter(controller, feet, kDt, world);
        }

        CHECK(feet.x <= 2.0f - controller.radius + 0.01f);
        CHECK(feet.z <= 2.0f - controller.radius + 0.01f);
    }
}

TEST_CASE("capsule: Fly mode, toggleMode, enabled = false, dt = 0, and determinism") {
    MeshCollider world = makeRampWorld(30.0f);
    // Ceiling at y = 10
    addQuad(world, {-20.0f, 10.0f, -20.0f}, {20.0f, 10.0f, -20.0f}, {20.0f, 10.0f, 20.0f},
            {-20.0f, 10.0f, 20.0f});
    world.build();

    constexpr float kDt = 1.0f / 60.0f;

    SUBCASE("Fly mode has no gravity and stops against mesh ceiling") {
        CapsuleCharacterController controller;
        controller.setMode(CharacterMode::Fly);
        glm::vec3 feet(0.0f, 5.0f, 0.0f);

        for (int i = 0; i < 30; ++i) {
            feet = stepCapsuleCharacter(controller, feet, kDt, world);
        }
        CHECK(feet.y == doctest::Approx(5.0f));

        controller.input.moveUp = 1.0f;
        for (int i = 0; i < 90; ++i) {
            feet = stepCapsuleCharacter(controller, feet, kDt, world);
        }

        const float expectedMaxFeetY = 10.0f - controller.height - controller.skin;
        CHECK(feet.y == doctest::Approx(expectedMaxFeetY).epsilon(0.02f));
        CHECK(controller.velocity.y == doctest::Approx(0.0f));
    }

    SUBCASE("toggleMode, enabled = false, and dt = 0") {
        CapsuleCharacterController controller;
        controller.velocity = glm::vec3(0.0f, -10.0f, 0.0f);

        controller.toggleMode();
        CHECK(controller.mode == CharacterMode::Fly);
        CHECK(controller.velocity.y == doctest::Approx(0.0f));
        controller.toggleMode();
        CHECK(controller.mode == CharacterMode::Walk);

        controller.enabled = false;
        controller.input.moveForward = 1.0f;
        const glm::vec3 orig(1.0f, 2.0f, 3.0f);
        glm::vec3 out = stepCapsuleCharacter(controller, orig, kDt, world);
        CHECK(out == orig);
        CHECK(controller.previousPosition == orig);

        controller.enabled = true;
        out = stepCapsuleCharacter(controller, orig, 0.0f, world);
        CHECK(out == orig);
    }

    SUBCASE("Determinism across two 180-step runs") {
        auto simulate = [&](glm::vec3& outFeet, CapsuleCharacterController& outCtrl) {
            CapsuleCharacterController ctrl;
            ctrl.yaw = 0.0f;
            glm::vec3 feet(-2.0f, 2.0f, 0.0f);
            for (int i = 0; i < 180; ++i) {
                ctrl.input.moveForward = (i > 30) ? 1.0f : 0.0f;
                ctrl.input.jump = (i >= 60 && i <= 65);
                ctrl.input.sprint = (i > 100);
                if (i == 90) {
                    ctrl.look(10.0f, -5.0f);
                }
                feet = stepCapsuleCharacter(ctrl, feet, kDt, world);
            }
            outFeet = feet;
            outCtrl = ctrl;
        };

        glm::vec3 feetA{0.0f};
        CapsuleCharacterController ctrlA;
        simulate(feetA, ctrlA);

        glm::vec3 feetB{0.0f};
        CapsuleCharacterController ctrlB;
        simulate(feetB, ctrlB);

        CHECK(feetA.x == feetB.x);
        CHECK(feetA.y == feetB.y);
        CHECK(feetA.z == feetB.z);
        CHECK(ctrlA.velocity.x == ctrlB.velocity.x);
        CHECK(ctrlA.velocity.y == ctrlB.velocity.y);
        CHECK(ctrlA.velocity.z == ctrlB.velocity.z);
        CHECK(ctrlA.grounded == ctrlB.grounded);
    }
}

TEST_CASE("capsule: ECS systems addCapsuleCharacterSystem and addCapsuleCharacterCameraSystem") {
    const MeshCollider floor = makeFlatFloor(0.0f);
    World world;

    const auto player = world.registry().create();
    world.registry().add<Transform>(player, glm::vec3(0.0f, 5.0f, 0.0f));
    world.registry().add<CapsuleCharacterController>(player);

    addCapsuleCharacterSystem(world, floor);
    world.run(SystemPhase::Fixed, 1.0f / 60.0f);

    const auto& playerTransform = world.registry().get<Transform>(player);
    CHECK(playerTransform.position.y < 5.0f);

    const auto camera = world.registry().create();
    world.registry().add<Transform>(camera);
    world.registry().add<Camera>(camera);
    world.registry().add<CapsuleCharacterCamera>(camera, CapsuleCharacterCamera{player});

    Time time;
    addCapsuleCharacterCameraSystem(world, time);
    world.run(SystemPhase::Render, 1.0f / 60.0f);

    const auto& camTransform = world.registry().get<Transform>(camera);
    const auto& currentCtrl = world.registry().get<CapsuleCharacterController>(player);
    CHECK(camTransform.position.y ==
          doctest::Approx(currentCtrl.previousPosition.y + currentCtrl.eyeHeight));

    Transform trFinal{glm::vec3(10.0f, 20.0f, 30.0f)};
    CapsuleCharacterController interpCtrl;
    interpCtrl.previousPosition = glm::vec3(0.0f, 10.0f, 20.0f);
    interpCtrl.eyeHeight = 1.62f;

    const glm::vec3 eyeHalf = capsuleCharacterEyePosition(interpCtrl, trFinal, 0.5f);
    CHECK(eyeHalf.x == doctest::Approx(5.0f));
    CHECK(eyeHalf.y == doctest::Approx(15.0f + 1.62f));
    CHECK(eyeHalf.z == doctest::Approx(25.0f));
}
