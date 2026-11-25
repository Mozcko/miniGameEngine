#include <doctest/doctest.h>
#include <mge/core/Camera2D.h>

#include <cmath>

using namespace mge;

namespace {
    constexpr float kPi = 3.14159265358979323846f;

    glm::vec3 toNdc(const glm::mat4& viewProjection, const glm::vec2& worldPos) {
        const glm::vec4 clip = viewProjection * glm::vec4(worldPos, 0.0f, 1.0f);
        return glm::vec3(clip) / clip.w;
    }
}

TEST_CASE("Camera2D: worldToScreen and screenToWorld round-trip with position, zoom, and rotation") {
    const glm::vec2 viewport(800.0f, 600.0f);
    Camera2D cam({100.0f, 50.0f}, 2.0f);

    // Camera position maps to the center of the viewport
    const glm::vec2 center = cam.worldToScreen(cam.position(), viewport);
    CHECK(center.x == doctest::Approx(400.0f));
    CHECK(center.y == doctest::Approx(300.0f));

    // +10 world X at 2x zoom is +20 screen pixels right; +15 world Y is +30 screen pixels down
    const glm::vec2 offset = cam.worldToScreen({110.0f, 65.0f}, viewport);
    CHECK(offset.x == doctest::Approx(420.0f));
    CHECK(offset.y == doctest::Approx(330.0f));

    // Round-trip without rotation
    const glm::vec2 worldPoint(137.5f, -42.25f);
    const glm::vec2 screen = cam.worldToScreen(worldPoint, viewport);
    const glm::vec2 back = cam.screenToWorld(screen, viewport);
    CHECK(back.x == doctest::Approx(worldPoint.x).epsilon(1e-4));
    CHECK(back.y == doctest::Approx(worldPoint.y).epsilon(1e-4));

    // Round-trip with rotation (90 degrees)
    cam.setRotation(kPi * 0.5f);
    const glm::vec2 rotatedScreen = cam.worldToScreen({110.0f, 50.0f}, viewport);
    // Rotating world delta (+10, 0) by -90 deg gives (0, -10), * 2x zoom = (0, -20) from center
    CHECK(rotatedScreen.x == doctest::Approx(400.0f).epsilon(1e-4));
    CHECK(rotatedScreen.y == doctest::Approx(280.0f).epsilon(1e-4));

    const glm::vec2 rotatedBack = cam.screenToWorld(rotatedScreen, viewport);
    CHECK(rotatedBack.x == doctest::Approx(110.0f).epsilon(1e-4));
    CHECK(rotatedBack.y == doctest::Approx(50.0f).epsilon(1e-4));
}

TEST_CASE("Camera2D: viewMatrix and viewProjectionMatrix match worldToScreen and NDC conventions") {
    const glm::vec2 viewport(800.0f, 600.0f);
    Camera2D cam({50.0f, 25.0f}, 2.0f);
    cam.setRotation(0.35f);

    const glm::mat4 view = cam.viewMatrix(viewport);
    const glm::vec2 sample(120.0f, -15.0f);
    const glm::vec2 expectedScreen = cam.worldToScreen(sample, viewport);
    const glm::vec4 matrixScreen = view * glm::vec4(sample, 0.0f, 1.0f);
    CHECK(matrixScreen.x == doctest::Approx(expectedScreen.x).epsilon(1e-4));
    CHECK(matrixScreen.y == doctest::Approx(expectedScreen.y).epsilon(1e-4));

    // Unrotated viewProjection: camera center -> NDC (0, 0), top-left -> (-1, +1), bottom-right -> (+1, -1)
    cam.setRotation(0.0f);
    const glm::mat4 vp = cam.viewProjectionMatrix(viewport);
    const glm::vec3 ndcCenter = toNdc(vp, cam.position());
    CHECK(ndcCenter.x == doctest::Approx(0.0f).epsilon(1e-4));
    CHECK(ndcCenter.y == doctest::Approx(0.0f).epsilon(1e-4));
    CHECK(ndcCenter.z == doctest::Approx(0.5f).epsilon(1e-4));

    const glm::vec2 topLeftWorld = cam.screenToWorld({0.0f, 0.0f}, viewport);
    const glm::vec2 bottomRightWorld = cam.screenToWorld(viewport, viewport);
    const glm::vec3 ndcTopLeft = toNdc(vp, topLeftWorld);
    const glm::vec3 ndcBottomRight = toNdc(vp, bottomRightWorld);
    CHECK(ndcTopLeft.x == doctest::Approx(-1.0f).epsilon(1e-4));
    CHECK(ndcTopLeft.y == doctest::Approx(1.0f).epsilon(1e-4));
    CHECK(ndcBottomRight.x == doctest::Approx(1.0f).epsilon(1e-4));
    CHECK(ndcBottomRight.y == doctest::Approx(-1.0f).epsilon(1e-4));

    // Zero viewport does not produce NaN
    CHECK(cam.viewProjectionMatrix({0.0f, 0.0f}) == glm::mat4(1.0f));
}

TEST_CASE("Camera2D: zoom clamping, zoomBy, and cursor-anchored zoomAt") {
    const glm::vec2 viewport(800.0f, 600.0f);
    Camera2D cam({0.0f, 0.0f}, 1.0f);
    cam.minZoom = 0.25f;
    cam.maxZoom = 4.0f;

    cam.setZoom(100.0f);
    CHECK(cam.zoom() == doctest::Approx(4.0f));
    cam.setZoom(-5.0f);
    CHECK(cam.zoom() == doctest::Approx(0.25f));

    cam.setZoom(1.0f);
    cam.zoomBy(2.0f);
    CHECK(cam.zoom() == doctest::Approx(2.0f));
    cam.zoomBy(0.0f); // Ignored
    CHECK(cam.zoom() == doctest::Approx(2.0f));

    // zoomAt keeps the world point under the cursor fixed on screen
    cam.setPosition({40.0f, 30.0f});
    cam.setZoom(1.0f);
    const glm::vec2 cursor(600.0f, 150.0f);
    const glm::vec2 worldUnderCursor = cam.screenToWorld(cursor, viewport);

    cam.zoomAt(2.5f, cursor, viewport);
    CHECK(cam.zoom() == doctest::Approx(2.5f));
    const glm::vec2 worldAfter = cam.screenToWorld(cursor, viewport);
    CHECK(worldAfter.x == doctest::Approx(worldUnderCursor.x).epsilon(1e-4));
    CHECK(worldAfter.y == doctest::Approx(worldUnderCursor.y).epsilon(1e-4));
}

TEST_CASE("Camera2D: visibleBounds and world bounds clamping") {
    const glm::vec2 viewport(800.0f, 600.0f);
    Camera2D cam({100.0f, 200.0f}, 2.0f);

    // At 2x zoom, 800x600 viewport sees 400x300 world units centered at (100, 200)
    const Bounds2D vis = cam.visibleBounds(viewport);
    CHECK(vis.min.x == doctest::Approx(-100.0f));
    CHECK(vis.max.x == doctest::Approx(300.0f));
    CHECK(vis.min.y == doctest::Approx(50.0f));
    CHECK(vis.max.y == doctest::Approx(350.0f));
    CHECK(vis.contains({100.0f, 200.0f}));
    CHECK_FALSE(vis.contains({301.0f, 200.0f}));

    // Clamp to a level [0, 1000] x [0, 800]
    cam.setBounds({{0.0f, 0.0f}, {1000.0f, 800.0f}});
    cam.setPosition({50.0f, 50.0f}); // Too close to top-left corner
    cam.clampToBounds(viewport);
    CHECK(cam.position().x == doctest::Approx(200.0f));
    CHECK(cam.position().y == doctest::Approx(150.0f));

    cam.setPosition({990.0f, 790.0f}); // Too close to bottom-right corner
    cam.clampToBounds(viewport);
    CHECK(cam.position().x == doctest::Approx(800.0f));
    CHECK(cam.position().y == doctest::Approx(650.0f));

    // Room smaller than viewport: camera centers on the room
    cam.setBounds({{10.0f, 20.0f}, {110.0f, 80.0f}});
    cam.setPosition({0.0f, 0.0f});
    cam.clampToBounds(viewport);
    CHECK(cam.position().x == doctest::Approx(60.0f));
    CHECK(cam.position().y == doctest::Approx(50.0f));
}

TEST_CASE("Camera2D: follow with deadzone and frame-rate independent smoothing") {
    Camera2D cam({0.0f, 0.0f});
    cam.deadzoneHalfSize = {20.0f, 10.0f};
    cam.followSmoothing = 0.0f; // Instant

    // Target inside deadzone: camera does not move
    cam.follow({15.0f, -8.0f}, 0.016f);
    CHECK(cam.position().x == doctest::Approx(0.0f));
    CHECK(cam.position().y == doctest::Approx(0.0f));

    // Target on deadzone boundary: camera does not move
    cam.follow({20.0f, 10.0f}, 0.016f);
    CHECK(cam.position().x == doctest::Approx(0.0f));
    CHECK(cam.position().y == doctest::Approx(0.0f));

    // Target exits deadzone on +X and -Y: camera shifts just enough to keep target on the edge
    cam.follow({35.0f, -25.0f}, 0.016f);
    CHECK(cam.position().x == doctest::Approx(15.0f));
    CHECK(cam.position().y == doctest::Approx(-15.0f));

    // Paused (dt <= 0): does not move
    cam.follow({500.0f, 500.0f}, 0.0f);
    CHECK(cam.position().x == doctest::Approx(15.0f));
    CHECK(cam.position().y == doctest::Approx(-15.0f));

    // Frame-rate independent exponential smoothing (no deadzone)
    Camera2D cam60({0.0f, 0.0f});
    Camera2D cam240({0.0f, 0.0f});
    cam60.followSmoothing = 8.0f;
    cam240.followSmoothing = 8.0f;

    const glm::vec2 target(100.0f, -50.0f);
    for (int i = 0; i < 60; ++i) cam60.follow(target, 1.0f / 60.0f);
    for (int i = 0; i < 240; ++i) cam240.follow(target, 1.0f / 240.0f);

    CHECK(cam60.position().x == doctest::Approx(cam240.position().x).epsilon(1e-3));
    CHECK(cam60.position().y == doctest::Approx(cam240.position().y).epsilon(1e-3));
    CHECK(cam60.position().x > 99.0f);
    CHECK(cam60.position().y < -49.0f);
}

TEST_CASE("Camera2D: deterministic trauma-based screen shake") {
    const glm::vec2 viewport(800.0f, 600.0f);
    Camera2D cam({50.0f, 50.0f});
    cam.maxShakeOffset = {20.0f, 10.0f};
    cam.maxShakeAngle = 0.1f;
    cam.shakeDecay = 2.0f;

    // Adding trauma clamps to [0, 1] and ignores non-positive values
    cam.addTrauma(-0.5f);
    CHECK(cam.trauma() == doctest::Approx(0.0f));
    cam.addTrauma(0.6f);
    cam.addTrauma(0.8f);
    CHECK(cam.trauma() == doctest::Approx(1.0f));

    // Update produces bounded non-zero offset without altering logical position()
    cam.update(0.016f);
    CHECK(cam.position() == glm::vec2(50.0f, 50.0f));
    CHECK(std::abs(cam.shakeOffset().x) <= 20.0f);
    CHECK(std::abs(cam.shakeOffset().y) <= 10.0f);
    CHECK(std::abs(cam.shakeAngle()) <= 0.1f);
    CHECK(cam.effectivePosition() == cam.position() + cam.shakeOffset());

    // Round-trip transform remains exact mid-shake
    const glm::vec2 p(123.0f, -45.0f);
    const glm::vec2 back = cam.screenToWorld(cam.worldToScreen(p, viewport), viewport);
    CHECK(back.x == doctest::Approx(p.x).epsilon(1e-4));
    CHECK(back.y == doctest::Approx(p.y).epsilon(1e-4));

    // Determinism: identical seed and steps produce identical shake sequence
    Camera2D a({0.0f, 0.0f});
    Camera2D b({0.0f, 0.0f});
    a.setShakeSeed(42);
    b.setShakeSeed(42);
    a.maxShakeAngle = 0.05f;
    b.maxShakeAngle = 0.05f;
    a.addTrauma(0.9f);
    b.addTrauma(0.9f);
    for (int i = 0; i < 10; ++i) {
        a.update(0.016f);
        b.update(0.016f);
        CHECK(a.shakeOffset() == b.shakeOffset());
        CHECK(a.shakeAngle() == b.shakeAngle());
    }

    // Trauma decays completely to zero and leaves no residual offset or drift
    for (int i = 0; i < 60; ++i) cam.update(0.016f);
    CHECK(cam.trauma() == doctest::Approx(0.0f));
    CHECK(cam.shakeOffset() == glm::vec2(0.0f));
    CHECK(cam.shakeAngle() == doctest::Approx(0.0f));
    CHECK(cam.position() == glm::vec2(50.0f, 50.0f));

    // stopShake clears active shake immediately
    cam.addTrauma(1.0f);
    cam.update(0.016f);
    cam.stopShake();
    CHECK(cam.trauma() == doctest::Approx(0.0f));
    CHECK(cam.shakeOffset() == glm::vec2(0.0f));
    CHECK(cam.shakeAngle() == doctest::Approx(0.0f));
}
