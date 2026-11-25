#include <doctest/doctest.h>
#include <mge/core/ThirdPersonCamera.h>
#include <mge/core/ecs/Camera.h>
#include <mge/core/math/Geometry.h>

#include <cmath>
#include <vector>

using namespace mge;

TEST_CASE("ThirdPersonCamera: default orientation, basis vectors, and Transform apply") {
    ThirdPersonCamera cam;
    CHECK(cam.yaw() == doctest::Approx(-90.0f));
    CHECK(cam.pitch() == doctest::Approx(-15.0f));
    CHECK(cam.flatForward().x == doctest::Approx(0.0f).epsilon(1e-5));
    CHECK(cam.flatForward().z == doctest::Approx(-1.0f).epsilon(1e-5));
    CHECK(cam.right().x == doctest::Approx(1.0f).epsilon(1e-5));
    CHECK(cam.right().z == doctest::Approx(0.0f).epsilon(1e-5));

    cam.snap({0.0f, 0.0f, 0.0f});
    CHECK(cam.pivot().y == doctest::Approx(cam.config().pivotOffset.y));
    CHECK(glm::distance(cam.position(), cam.pivot()) == doctest::Approx(cam.config().distance).epsilon(1e-4));
    // Negative pitch looks slightly downward from behind (+Z) and above (+Y)
    CHECK(cam.position().z > 0.0f);
    CHECK(cam.position().y > cam.pivot().y);

    Transform transform;
    cam.apply(transform);
    CHECK(transform.position == cam.position());
    const glm::vec3 lookDir = transform.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
    CHECK(glm::dot(lookDir, cam.forward()) == doctest::Approx(1.0f).epsilon(1e-4));
}

TEST_CASE("ThirdPersonCamera: orbit, pitch clamping, invertY, yaw wrap, and pause") {
    ThirdPersonCameraConfig cfg;
    cfg.rotationSmoothing = 0.0f;
    cfg.followSmoothing = 0.0f;
    cfg.sensitivity = 1.0f;
    ThirdPersonCamera cam(cfg, -90.0f, 0.0f);
    cam.snap({0.0f, 0.0f, 0.0f});

    // Orbit right 90 degrees: -90 -> 0 (+X)
    ThirdPersonCameraInput input;
    input.lookX = 90.0f;
    cam.update(0.016f, {0.0f, 0.0f, 0.0f}, input);
    CHECK(cam.yaw() == doctest::Approx(0.0f).epsilon(1e-4));
    CHECK(cam.flatForward().x == doctest::Approx(1.0f).epsilon(1e-4));

    // Pitch clamps to [minPitch, maxPitch]
    ThirdPersonCameraInput lookUp;
    lookUp.lookY = -1000.0f;
    cam.update(0.016f, {0.0f, 0.0f, 0.0f}, lookUp);
    CHECK(cam.pitch() == doctest::Approx(cfg.maxPitch));

    ThirdPersonCameraInput lookDown;
    lookDown.lookY = 1000.0f;
    cam.update(0.016f, {0.0f, 0.0f, 0.0f}, lookDown);
    CHECK(cam.pitch() == doctest::Approx(cfg.minPitch));

    // Extreme config pitch limits are still clamped inside [-89, 89]
    cam.config().minPitch = -120.0f;
    cam.config().maxPitch = 120.0f;
    cam.update(0.016f, {0.0f, 0.0f, 0.0f}, lookUp);
    CHECK(cam.pitch() == doctest::Approx(89.0f));
    cam.update(0.016f, {0.0f, 0.0f, 0.0f}, lookDown);
    CHECK(cam.pitch() == doctest::Approx(-89.0f));

    // invertY reverses vertical look direction
    cam.setAngles(0.0f, 0.0f);
    cam.config().invertY = true;
    ThirdPersonCameraInput smallUp;
    smallUp.lookY = -10.0f;
    cam.update(0.016f, {0.0f, 0.0f, 0.0f}, smallUp);
    CHECK(cam.pitch() == doctest::Approx(-10.0f).epsilon(1e-4));

    // Yaw wraps cleanly across +-180
    cam.setAngles(175.0f, 0.0f);
    ThirdPersonCameraInput wrapRight;
    wrapRight.lookX = 20.0f;
    cam.update(0.016f, {0.0f, 0.0f, 0.0f}, wrapRight);
    CHECK(cam.yaw() == doctest::Approx(-165.0f).epsilon(1e-4));

    // Paused (dt <= 0) neither moves nor rotates
    const glm::vec3 posBefore = cam.position();
    const float yawBefore = cam.yaw();
    const float pitchBefore = cam.pitch();
    ThirdPersonCameraInput bigInput{50.0f, 50.0f, 5.0f};
    cam.update(0.0f, {100.0f, 20.0f, -50.0f}, bigInput);
    cam.update(-0.1f, {100.0f, 20.0f, -50.0f}, bigInput);
    CHECK(cam.position() == posBefore);
    CHECK(cam.yaw() == yawBefore);
    CHECK(cam.pitch() == pitchBefore);
}

TEST_CASE("ThirdPersonCamera: zoom clamping and smoothing") {
    ThirdPersonCameraConfig cfg;
    cfg.distance = 5.0f;
    cfg.minDistance = 2.0f;
    cfg.maxDistance = 10.0f;
    cfg.zoomSpeed = 1.0f;
    cfg.zoomSmoothing = 0.0f;
    ThirdPersonCamera cam(cfg);
    cam.snap({0.0f, 0.0f, 0.0f});

    cam.update(0.016f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 2.0f});
    CHECK(cam.desiredDistance() == doctest::Approx(3.0f));
    CHECK(cam.currentDistance() == doctest::Approx(3.0f));

    cam.update(0.016f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 100.0f});
    CHECK(cam.desiredDistance() == doctest::Approx(2.0f));

    cam.update(0.016f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -100.0f});
    CHECK(cam.desiredDistance() == doctest::Approx(10.0f));

    // Smoothed zoom moves gradually toward desiredDistance
    cam.config().zoomSmoothing = 10.0f;
    cam.config().collisionRecoverSpeed = 0.0f;
    cam.setDistance(4.0f, false);
    cam.update(0.016f, {0.0f, 0.0f, 0.0f});
    CHECK(cam.currentDistance() < 10.0f);
    CHECK(cam.currentDistance() > 4.0f);
    for (int i = 0; i < 120; ++i) cam.update(0.016f, {0.0f, 0.0f, 0.0f});
    CHECK(cam.currentDistance() == doctest::Approx(4.0f).epsilon(1e-3));
}

TEST_CASE("ThirdPersonCamera: pivot follow, shoulder offset, and frame-rate independent smoothing") {
    // First update snaps pivot immediately (no fly-in from world origin)
    ThirdPersonCamera camA;
    camA.config().followSmoothing = 8.0f;
    camA.update(0.016f, {10.0f, 0.0f, 0.0f});
    CHECK(camA.pivot().x == doctest::Approx(10.0f));

    // Frame-rate independence: 1 step of 0.2s == 20 steps of 0.01s for a constant target
    ThirdPersonCamera camB;
    camB.config().followSmoothing = 8.0f;
    camB.snap({10.0f, 0.0f, 0.0f});

    camA.update(0.2f, {20.0f, 0.0f, 0.0f});
    for (int i = 0; i < 20; ++i) camB.update(0.01f, {20.0f, 0.0f, 0.0f});
    CHECK(camA.pivot().x == doctest::Approx(camB.pivot().x).epsilon(1e-4));
    CHECK(camA.pivot().x > 10.0f);
    CHECK(camA.pivot().x < 20.0f);

    // Shoulder offset shifts camera in camera-local right/up/forward basis
    ThirdPersonCameraConfig shoulderCfg;
    shoulderCfg.distance = 4.0f;
    shoulderCfg.pivotOffset = {0.0f, 1.5f, 0.0f};
    shoulderCfg.shoulderOffset = {1.0f, 0.5f, 0.0f};
    ThirdPersonCamera shoulderCam(shoulderCfg, -90.0f, 0.0f);
    shoulderCam.snap({0.0f, 0.0f, 0.0f});
    CHECK(shoulderCam.position().x == doctest::Approx(1.0f).epsilon(1e-4));
    CHECK(shoulderCam.position().y == doctest::Approx(2.0f).epsilon(1e-4));
    CHECK(shoulderCam.position().z == doctest::Approx(4.0f).epsilon(1e-4));
}

TEST_CASE("ThirdPersonCamera: spring-arm collision pull-in, floor, and smooth recovery") {
    ThirdPersonCameraConfig cfg;
    cfg.distance = 5.0f;
    cfg.collisionRadius = 0.25f;
    cfg.minCollisionDistance = 0.5f;
    cfg.collisionPullInSpeed = 0.0f; // Instant pull-in
    cfg.collisionRecoverSpeed = 8.0f;
    ThirdPersonCamera cam(cfg, -90.0f, 0.0f);

    std::optional<float> simulatedHit;
    cam.setCollisionProbe(
        [&simulatedHit](const glm::vec3&, const glm::vec3&, float) { return simulatedHit; });

    // Unobstructed
    cam.snap({0.0f, 0.0f, 0.0f});
    CHECK_FALSE(cam.isColliding());
    CHECK(cam.currentDistance() == doctest::Approx(5.0f));

    // Wall appears at t = 2.0 -> pulls in immediately to 2.0 - 0.25 = 1.75
    simulatedHit = 2.0f;
    cam.update(0.016f, {0.0f, 0.0f, 0.0f});
    CHECK(cam.isColliding());
    CHECK(cam.currentDistance() == doctest::Approx(1.75f).epsilon(1e-4));

    // Close wall at t = 0.6 -> clamped to minCollisionDistance (0.5), still in front of wall
    simulatedHit = 0.6f;
    cam.update(0.016f, {0.0f, 0.0f, 0.0f});
    CHECK(cam.currentDistance() == doctest::Approx(0.5f).epsilon(1e-4));

    // Wall closer than minCollisionDistance (t = 0.2) -> never places camera behind the wall
    simulatedHit = 0.2f;
    cam.update(0.016f, {0.0f, 0.0f, 0.0f});
    CHECK(cam.currentDistance() <= 0.2f + 1e-4f);

    // Wall disappears -> recovers smoothly rather than popping back in one frame
    simulatedHit.reset();
    cam.update(0.016f, {0.0f, 0.0f, 0.0f});
    CHECK_FALSE(cam.isColliding());
    CHECK(cam.currentDistance() > 0.2f);
    CHECK(cam.currentDistance() < 5.0f);

    for (int i = 0; i < 120; ++i) cam.update(0.016f, {0.0f, 0.0f, 0.0f});
    CHECK(cam.currentDistance() == doctest::Approx(5.0f).epsilon(1e-3));
}

TEST_CASE("ThirdPersonCamera: collision probe with Geometry::intersectRayAabb") {
    ThirdPersonCameraConfig cfg;
    cfg.distance = 5.0f;
    cfg.collisionRadius = 0.25f;
    cfg.pivotOffset = {0.0f, 1.5f, 0.0f};
    ThirdPersonCamera cam(cfg, -90.0f, 0.0f);

    // Wall behind the player at z in [2.5, 3.5]
    const Aabb wall({-5.0f, 0.0f, 2.5f}, {5.0f, 5.0f, 3.5f});
    cam.setCollisionProbe([&wall](const glm::vec3& origin, const glm::vec3& dir,
                                  float maxDist) -> std::optional<float> {
        const auto hit = intersectRayAabb(Ray(origin, dir), wall);
        if (hit && *hit <= maxDist) return hit;
        return std::nullopt;
    });

    cam.snap({0.0f, 0.0f, 0.0f});
    CHECK(cam.isColliding());
    CHECK(cam.position().z == doctest::Approx(2.25f).epsilon(1e-4));
}

TEST_CASE("ThirdPersonCamera: target lock-on tracking, framing, wrap-around, and auto-break") {
    ThirdPersonCameraConfig cfg;
    cfg.distance = 5.0f;
    cfg.pivotOffset = {0.0f, 1.5f, 0.0f};
    cfg.lockOnSmoothing = 12.0f;
    cfg.lockOnPitchOffset = -10.0f;
    cfg.lockOnMaxDistance = 30.0f;
    cfg.lockOnLookBias = 0.35f;
    ThirdPersonCamera cam(cfg, -90.0f, -15.0f);
    cam.snap({0.0f, 0.0f, 0.0f});

    // Lock onto enemy along +X (yaw = 0)
    const glm::vec3 enemyPos{10.0f, 1.5f, 0.0f};
    cam.setLockOnTarget(enemyPos);
    CHECK(cam.isLockedOn());

    // Manual look input is ignored while locked on, while zoom still works
    for (int i = 0; i < 120; ++i) {
        cam.update(1.0f / 60.0f, {0.0f, 0.0f, 0.0f}, {500.0f, 500.0f, 0.0f});
    }
    CHECK(cam.yaw() == doctest::Approx(0.0f).epsilon(1e-2));
    CHECK(cam.pitch() == doctest::Approx(-10.0f).epsilon(1e-2));

    // Both player pivot and enemy are inside the camera frustum
    Transform camTransform;
    cam.apply(camTransform);
    Camera cameraComponent;
    const CameraMatrices mats = cameraMatrices(camTransform.localMatrix(), cameraComponent, {1280, 720});
    CHECK(mats.frustum.contains(cam.pivot()));
    CHECK(mats.frustum.contains(enemyPos));

    // Releasing lock-on keeps current orientation (no snap back to -90)
    const float yawAtUnlock = cam.yaw();
    cam.clearLockOn();
    CHECK_FALSE(cam.isLockedOn());
    cam.update(1.0f / 60.0f, {0.0f, 0.0f, 0.0f});
    CHECK(cam.yaw() == doctest::Approx(yawAtUnlock).epsilon(1e-2));

    // Smooth shortest-path wrap across +-180 degrees
    cam.setAngles(170.0f, 0.0f);
    const float targetRad = glm::radians(-170.0f);
    cam.setLockOnTarget(cam.pivot() + glm::vec3(std::cos(targetRad), 0.0f, std::sin(targetRad)) * 10.0f);
    cam.update(0.016f, {0.0f, 0.0f, 0.0f});
    // Moved through +-180 (not backward through 0)
    CHECK(std::abs(cam.yaw()) > 170.0f);

    // Auto-break when target exceeds lockOnMaxDistance
    cam.setLockOnTarget({100.0f, 1.5f, 0.0f});
    cam.update(0.016f, {0.0f, 0.0f, 0.0f});
    CHECK_FALSE(cam.isLockedOn());

    // Degenerate target directly above pivot never produces NaN rotation
    cam.setLockOnTarget({0.0f, 20.0f, 0.0f});
    cam.snap({0.0f, 0.0f, 0.0f});
    Transform safeTransform;
    cam.apply(safeTransform);
    CHECK(std::isfinite(safeTransform.rotation.w));
    CHECK(std::isfinite(safeTransform.rotation.x));
    CHECK(std::isfinite(safeTransform.rotation.y));
    CHECK(std::isfinite(safeTransform.rotation.z));
}

TEST_CASE("ThirdPersonCamera: selectLockOnTarget and switchLockOnTarget helpers") {
    const glm::vec3 origin{0.0f, 0.0f, 0.0f};
    const glm::vec3 fwd{0.0f, 0.0f, -1.0f};
    const glm::vec3 rightVec{1.0f, 0.0f, 0.0f};

    CHECK_FALSE(selectLockOnTarget(origin, fwd, {}).has_value());

    const std::vector<glm::vec3> candidates = {
        {-4.0f, 0.0f, -10.0f}, // 0: left
        {0.0f, 0.0f, -10.0f},  // 1: dead center
        {4.0f, 0.0f, -10.0f},  // 2: right
        {10.0f, 0.0f, -10.0f}, // 3: far right
        {0.0f, 0.0f, 5.0f},    // 4: behind camera
        {0.0f, 0.0f, -50.0f},  // 5: out of range
    };

    const auto picked = selectLockOnTarget(origin, fwd, candidates, 30.0f, 60.0f);
    REQUIRE(picked.has_value());
    CHECK(*picked == 1u);

    // Switch right from center (1) -> nearest right (2) -> far right (3) -> none
    const auto right1 = switchLockOnTarget(origin, fwd, rightVec, candidates, 1, +1, 30.0f);
    REQUIRE(right1.has_value());
    CHECK(*right1 == 2u);

    const auto right2 = switchLockOnTarget(origin, fwd, rightVec, candidates, 2, +1, 30.0f);
    REQUIRE(right2.has_value());
    CHECK(*right2 == 3u);

    CHECK_FALSE(switchLockOnTarget(origin, fwd, rightVec, candidates, 3, +1, 30.0f).has_value());

    // Switch left from center (1) -> left (0)
    const auto left1 = switchLockOnTarget(origin, fwd, rightVec, candidates, 1, -1, 30.0f);
    REQUIRE(left1.has_value());
    CHECK(*left1 == 0u);

    // Zero direction returns nullopt
    CHECK_FALSE(switchLockOnTarget(origin, fwd, rightVec, candidates, 1, 0, 30.0f).has_value());

    // Only invalid candidates (behind or out of range) returns nullopt
    const std::vector<glm::vec3> invalidOnly = {{0.0f, 0.0f, 5.0f}, {0.0f, 0.0f, -50.0f}};
    CHECK_FALSE(selectLockOnTarget(origin, fwd, invalidOnly, 30.0f, 60.0f).has_value());
}
