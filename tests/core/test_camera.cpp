#include <doctest/doctest.h>
#include <mge/core/Application.h>
#include <mge/core/FlyController.h>
#include <mge/core/ecs/Camera.h>
#include <mge/core/ecs/Registry.h>

#include <glm/gtc/matrix_transform.hpp>

using namespace mge;

namespace {
    // World point → NDC (x, y in [-1, 1], z in [0, 1])
    glm::vec3 toNdc(const glm::mat4& viewProjection, const glm::vec3& p) {
        const glm::vec4 clip = viewProjection * glm::vec4(p, 1.0f);
        return glm::vec3(clip) / clip.w;
    }
}

TEST_CASE("Camera: perspective with Z in [0, 1], looking toward local -Z") {
    Camera camera;
    camera.aspect = 1.0f;
    const CameraMatrices m = cameraMatrices(glm::mat4(1.0f), camera, {100, 100});
    CHECK(toNdc(m.viewProjection, {0, 0, -camera.nearPlane}).z == doctest::Approx(0.0f).epsilon(1e-4));
    CHECK(toNdc(m.viewProjection, {0, 0, -camera.farPlane}).z == doctest::Approx(1.0f).epsilon(1e-4));
    const glm::vec3 center = toNdc(m.viewProjection, {0, 0, -10});
    CHECK(center.x == doctest::Approx(0.0f));
    CHECK(center.y == doctest::Approx(0.0f));
    CHECK(toNdc(m.viewProjection, {0, 1, -10}).y > 0.0f); // +Y up in NDC
    CHECK(m.position == glm::vec3(0.0f));
}

TEST_CASE("Camera: camera transform is inverted for the view matrix") {
    Transform transform;
    transform.position = {5, 2, 3};
    Camera camera;
    const CameraMatrices m = cameraMatrices(transform.localMatrix(), camera, {1280, 720});
    CHECK(m.position == glm::vec3(5, 2, 3));
    const glm::vec3 inFront = toNdc(m.viewProjection, {5, 2, -7});
    CHECK(inFront.x == doctest::Approx(0.0f).epsilon(1e-4));
    CHECK(inFront.y == doctest::Approx(0.0f).epsilon(1e-4));
    CHECK(m.frustum.contains({5, 2, -7}));
    CHECK_FALSE(m.frustum.contains({5, 2, 13})); // Behind
}

TEST_CASE("Camera: pixel orthographic projection, (0, 0) at top-left") {
    Camera camera;
    camera.projection = ProjectionType::Orthographic;
    camera.pixelSpace = true;
    camera.nearPlane = -1.0f;
    camera.farPlane = 1.0f;
    const glm::mat4 projection = projectionMatrix(camera, {800, 600});
    const glm::vec3 topLeft = toNdc(projection, {0, 0, 0});
    const glm::vec3 bottomRight = toNdc(projection, {800, 600, 0});
    CHECK(topLeft.x == doctest::Approx(-1.0f));
    CHECK(topLeft.y == doctest::Approx(1.0f));
    CHECK(bottomRight.x == doctest::Approx(1.0f));
    CHECK(bottomRight.y == doctest::Approx(-1.0f));
}

TEST_CASE("Camera: world-space orthographic projection with orthoHeight and aspect") {
    Camera camera;
    camera.projection = ProjectionType::Orthographic;
    camera.orthoHeight = 10.0f;
    camera.aspect = 2.0f;
    const glm::mat4 projection = projectionMatrix(camera, {200, 100});
    CHECK(toNdc(projection, {10, 5, -1}).x == doctest::Approx(1.0f));
    CHECK(toNdc(projection, {10, 5, -1}).y == doctest::Approx(1.0f));
}

TEST_CASE("CameraSystem: aspect follows viewport (only with autoAspect)") {
    Registry registry;
    const Entity automatic = registry.create();
    registry.add<Camera>(automatic);
    const Entity fixed = registry.create();
    registry.add<Camera>(fixed).autoAspect = false;
    registry.get<Camera>(fixed).aspect = 1.0f;

    CameraSystem::update(registry, {1920, 1080});
    CHECK(registry.get<Camera>(automatic).aspect == doctest::Approx(1920.0f / 1080.0f));
    CHECK(registry.get<Camera>(fixed).aspect == doctest::Approx(1.0f));
    CameraSystem::update(registry, {0, 0}); // Minimized: preserved
    CHECK(registry.get<Camera>(automatic).aspect == doctest::Approx(1920.0f / 1080.0f));
}

TEST_CASE("Application: viewport starts with config size and loop updates cameras") {
    ApplicationConfig config;
    config.width = 640;
    config.height = 480;
    config.maxFrames = 1;
    Application app(config);
    CHECK(app.viewport() == glm::uvec2(640, 480));
    const Entity camera = app.world().registry().create();
    app.world().registry().add<Camera>(camera);
    app.setViewport({300, 100});
    CHECK(app.run() == 0);
    CHECK(app.world().registry().get<Camera>(camera).aspect == doctest::Approx(3.0f));
}

TEST_CASE("Frustum: near plane matches the projection Z range") {
    const glm::mat4 view = glm::lookAt(glm::vec3(0.0f), glm::vec3(0, 0, -1), glm::vec3(0, 1, 0));
    const Frustum zo = Frustum::fromViewProjection(glm::perspectiveRH_ZO(1.0f, 1.0f, 1.0f, 100.0f) * view,
                                                   DepthRange::ZeroToOne);
    const Frustum gl = Frustum::fromViewProjection(glm::perspectiveRH_NO(1.0f, 1.0f, 1.0f, 100.0f) * view,
                                                   DepthRange::MinusOneToOne);
    for (const Frustum* f : {&zo, &gl}) {
        CHECK(f->contains({0, 0, -1.5f}));
        CHECK_FALSE(f->contains({0, 0, -0.5f})); // Before near plane
        CHECK(f->contains({0, 0, -99.0f}));
        CHECK_FALSE(f->contains({0, 0, -101.0f})); // Beyond far plane
    }
    // A ZO projection read as GL would place the near plane in the wrong position
    const Frustum wrong = Frustum::fromViewProjection(glm::perspectiveRH_ZO(1.0f, 1.0f, 1.0f, 100.0f) * view,
                                                      DepthRange::MinusOneToOne);
    CHECK(wrong.contains({0, 0, -0.7f}));
}

TEST_CASE("FlyController: looks, moves along view direction, and respects pause") {
    FlyController fly({0, 0, 0});
    CHECK(fly.forward().z == doctest::Approx(-1.0f)); // yaw -90 = toward -Z
    CameraInput input;
    input.moveForward = 1.0f;
    fly.update(1.0f, input);
    CHECK(fly.position().z == doctest::Approx(-fly.speed));
    fly.update(0.0f, input); // Paused: does not move
    CHECK(fly.position().z == doctest::Approx(-fly.speed));

    CameraInput look;
    look.lookY = -10000.0f; // Large upward look: pitch clamps at 89
    fly.update(0.016f, look);
    CHECK(fly.pitch() == doctest::Approx(89.0f));

    // apply: camera (local -Z) points toward forward()
    Transform transform;
    fly.apply(transform);
    const glm::vec3 lookDir = transform.rotation * glm::vec3(0, 0, -1);
    CHECK(glm::dot(lookDir, fly.forward()) == doctest::Approx(1.0f).epsilon(1e-4));
    CHECK(transform.position == fly.position());
}

TEST_CASE("FlyController: axis-by-axis collisions (slides along wall)") {
    FlyController fly({0, 0, 0}, 0.0f); // yaw 0 = toward +X
    // Wall at x >= 1
    fly.setCollisionCallback([](const glm::vec3&, const glm::vec3& max) { return max.x >= 1.0f; });
    CameraInput input;
    input.moveForward = 1.0f;
    input.moveRight = 1.0f; // Diagonal: X collides, Z continues
    for (int i = 0; i < 20; i++) fly.update(0.05f, input);
    CHECK(fly.position().x < 1.0f - fly.halfExtents.x + 1e-3f);
    CHECK(fly.position().z > 1.0f);
}
