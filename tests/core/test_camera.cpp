#include <doctest/doctest.h>
#include <mge/core/Application.h>
#include <mge/core/FlyController.h>
#include <mge/core/ecs/Camera.h>
#include <mge/core/ecs/Registry.h>

#include <glm/gtc/matrix_transform.hpp>

using namespace mge;

namespace {
    // Punto del mundo → NDC (x, y en [-1, 1], z en [0, 1])
    glm::vec3 toNdc(const glm::mat4& viewProjection, const glm::vec3& p) {
        const glm::vec4 clip = viewProjection * glm::vec4(p, 1.0f);
        return glm::vec3(clip) / clip.w;
    }
}

TEST_CASE("Camera: perspectiva con Z en [0, 1], mirando hacia -Z local") {
    Camera camera;
    camera.aspect = 1.0f;
    const CameraMatrices m = cameraMatrices(glm::mat4(1.0f), camera, {100, 100});
    CHECK(toNdc(m.viewProjection, {0, 0, -camera.nearPlane}).z == doctest::Approx(0.0f).epsilon(1e-4));
    CHECK(toNdc(m.viewProjection, {0, 0, -camera.farPlane}).z == doctest::Approx(1.0f).epsilon(1e-4));
    const glm::vec3 center = toNdc(m.viewProjection, {0, 0, -10});
    CHECK(center.x == doctest::Approx(0.0f));
    CHECK(center.y == doctest::Approx(0.0f));
    CHECK(toNdc(m.viewProjection, {0, 1, -10}).y > 0.0f); // +Y arriba en NDC
    CHECK(m.position == glm::vec3(0.0f));
}

TEST_CASE("Camera: el transform de la cámara se invierte para la vista") {
    Transform transform;
    transform.position = {5, 2, 3};
    Camera camera;
    const CameraMatrices m = cameraMatrices(transform.localMatrix(), camera, {1280, 720});
    CHECK(m.position == glm::vec3(5, 2, 3));
    const glm::vec3 inFront = toNdc(m.viewProjection, {5, 2, -7});
    CHECK(inFront.x == doctest::Approx(0.0f).epsilon(1e-4));
    CHECK(inFront.y == doctest::Approx(0.0f).epsilon(1e-4));
    CHECK(m.frustum.contains({5, 2, -7}));
    CHECK_FALSE(m.frustum.contains({5, 2, 13})); // Detrás
}

TEST_CASE("Camera: ortográfica en píxeles, (0, 0) arriba a la izquierda") {
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

TEST_CASE("Camera: ortográfica en unidades del mundo con orthoHeight y aspecto") {
    Camera camera;
    camera.projection = ProjectionType::Orthographic;
    camera.orthoHeight = 10.0f;
    camera.aspect = 2.0f;
    const glm::mat4 projection = projectionMatrix(camera, {200, 100});
    CHECK(toNdc(projection, {10, 5, -1}).x == doctest::Approx(1.0f));
    CHECK(toNdc(projection, {10, 5, -1}).y == doctest::Approx(1.0f));
}

TEST_CASE("CameraSystem: el aspecto sigue al viewport (solo con autoAspect)") {
    Registry registry;
    const Entity automatic = registry.create();
    registry.add<Camera>(automatic);
    const Entity fixed = registry.create();
    registry.add<Camera>(fixed).autoAspect = false;
    registry.get<Camera>(fixed).aspect = 1.0f;

    CameraSystem::update(registry, {1920, 1080});
    CHECK(registry.get<Camera>(automatic).aspect == doctest::Approx(1920.0f / 1080.0f));
    CHECK(registry.get<Camera>(fixed).aspect == doctest::Approx(1.0f));
    CameraSystem::update(registry, {0, 0}); // Minimizada: se conserva
    CHECK(registry.get<Camera>(automatic).aspect == doctest::Approx(1920.0f / 1080.0f));
}

TEST_CASE("Application: el viewport arranca con el tamaño del config y el loop ajusta las cámaras") {
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

TEST_CASE("Frustum: plano cercano según el rango de Z de la proyección") {
    const glm::mat4 view = glm::lookAt(glm::vec3(0.0f), glm::vec3(0, 0, -1), glm::vec3(0, 1, 0));
    const Frustum zo = Frustum::fromViewProjection(glm::perspectiveRH_ZO(1.0f, 1.0f, 1.0f, 100.0f) * view,
                                                   DepthRange::ZeroToOne);
    const Frustum gl = Frustum::fromViewProjection(glm::perspectiveRH_NO(1.0f, 1.0f, 1.0f, 100.0f) * view,
                                                   DepthRange::MinusOneToOne);
    for (const Frustum* f : {&zo, &gl}) {
        CHECK(f->contains({0, 0, -1.5f}));
        CHECK_FALSE(f->contains({0, 0, -0.5f})); // Antes del plano cercano
        CHECK(f->contains({0, 0, -99.0f}));
        CHECK_FALSE(f->contains({0, 0, -101.0f})); // Después del lejano
    }
    // Una proyección ZO leída como si fuera GL tendría el plano cercano en el lugar equivocado
    const Frustum wrong = Frustum::fromViewProjection(glm::perspectiveRH_ZO(1.0f, 1.0f, 1.0f, 100.0f) * view,
                                                      DepthRange::MinusOneToOne);
    CHECK(wrong.contains({0, 0, -0.7f}));
}

TEST_CASE("FlyController: mira, se mueve en la dirección de la vista y respeta la pausa") {
    FlyController fly({0, 0, 0});
    CHECK(fly.forward().z == doctest::Approx(-1.0f)); // yaw -90 = hacia -Z
    CameraInput input;
    input.moveForward = 1.0f;
    fly.update(1.0f, input);
    CHECK(fly.position().z == doctest::Approx(-fly.speed));
    fly.update(0.0f, input); // Pausa: no se mueve
    CHECK(fly.position().z == doctest::Approx(-fly.speed));

    CameraInput look;
    look.lookY = -10000.0f; // Mucho hacia arriba: el pitch queda en 89
    fly.update(0.016f, look);
    CHECK(fly.pitch() == doctest::Approx(89.0f));

    // apply: la cámara (−Z local) apunta a forward()
    Transform transform;
    fly.apply(transform);
    const glm::vec3 lookDir = transform.rotation * glm::vec3(0, 0, -1);
    CHECK(glm::dot(lookDir, fly.forward()) == doctest::Approx(1.0f).epsilon(1e-4));
    CHECK(transform.position == fly.position());
}

TEST_CASE("FlyController: colisiones eje por eje (se desliza por la pared)") {
    FlyController fly({0, 0, 0}, 0.0f); // yaw 0 = hacia +X
    // Pared en x >= 1
    fly.setCollisionCallback([](const glm::vec3&, const glm::vec3& max) { return max.x >= 1.0f; });
    CameraInput input;
    input.moveForward = 1.0f;
    input.moveRight = 1.0f; // Diagonal: X choca, Z sigue
    for (int i = 0; i < 20; i++) fly.update(0.05f, input);
    CHECK(fly.position().x < 1.0f - fly.halfExtents.x + 1e-3f);
    CHECK(fly.position().z > 1.0f);
}
