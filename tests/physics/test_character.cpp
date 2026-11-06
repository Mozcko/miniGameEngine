#include <doctest/doctest.h>
#include <mge/core/Time.h>
#include <mge/core/ecs/Transform.h>
#include <mge/core/ecs/World.h>
#include <mge/core/ecs/Camera.h>
#include <mge/physics/CharacterController.h>
#include <mge/physics/GridQuery.h>

#include <algorithm>

using namespace mge;

TEST_CASE("character: walk parado sobre el piso se queda grounded y quieto sin hundirse ni vibrar") {
    // Suelo en y = 0 (bloques con y <= 0 son sólidos, cara superior en y = 1.0)
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

TEST_CASE("character: cae desde y = 10, aterriza, grounded y velocity.y == 0") {
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

TEST_CASE("character: salto alcanza la altura esperada y vuelve a caer") {
    auto isSolid = [](const glm::ivec3& p) { return p.y <= 0; };

    constexpr float kSkin = 1e-4f;
    CharacterController controller;
    glm::vec3 feet(0.0f, 1.0f + kSkin, 0.0f);
    controller.grounded = true;

    // Altura esperada según cinemática: v^2 / (2 * g)
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

TEST_CASE("character: camina hacia adelante a walkSpeed en direccion flatForward ignorando pitch") {
    auto isSolid = [](const glm::ivec3& p) { return p.y <= 0; };

    constexpr float kSkin = 1e-4f;
    CharacterController controller;
    glm::vec3 feet(0.0f, 1.0f + kSkin, 0.0f);
    controller.grounded = true;

    // Mirar hacia abajo 45 grados (pitch = -45) con yaw = -90 (mira hacia -Z)
    controller.look(0.0f, -45.0f);
    controller.input.moveForward = 1.0f;

    constexpr float kDt = 1.0f / 60.0f;
    // Con aceleración de 40 m/s^2, alcanza 4.3 m/s en aprox 0.11 s (7 frames)
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

TEST_CASE("character: step-up sube escalon de 1 bloque con stepHeight 1.0 pero no con 0.6") {
    // Suelo en y = 0 para z > 0; escalon de 1 bloque (y = 1) para z <= 0
    auto isSolid = [](const glm::ivec3& p) { return p.y <= 0 || (p.y == 1 && p.z <= 0); };

    constexpr float kSkin = 1e-4f;
    constexpr float kDt = 1.0f / 60.0f;

    // Caso A: con stepHeight = 1.05f puede subir el escalón de 1.0 unidad
    {
        CharacterController controller;
        controller.stepHeight = 1.05f;
        glm::vec3 feet(0.0f, 1.0f + kSkin, 1.0f);
        controller.grounded = true;
        controller.input.moveForward = 1.0f;

        for (int i = 0; i < 60; ++i) {
            feet = stepCharacter(controller, feet, kDt, isSolid);
        }

        // Debe haber subido a la cima del bloque y=1 (cara superior en y=2.0)
        CHECK(feet.y == doctest::Approx(2.0f + kSkin));
        CHECK(feet.z < 0.0f);
        CHECK(controller.grounded);
    }

    // Caso B: con stepHeight = 0.6f (default) no puede subir el escalón de 1.0 unidad
    {
        CharacterController controller;
        controller.stepHeight = 0.6f;
        glm::vec3 feet(0.0f, 1.0f + kSkin, 1.0f);
        controller.grounded = true;
        controller.input.moveForward = 1.0f;

        for (int i = 0; i < 60; ++i) {
            feet = stepCharacter(controller, feet, kDt, isSolid);
        }

        // Queda bloqueado frente al escalón sobre el suelo original
        CHECK(feet.y == doctest::Approx(1.0f + kSkin));
        CHECK(feet.z > 0.0f);
    }
}

TEST_CASE("character: modo Fly no tiene gravedad, moveUp sube y frena con colision") {
    constexpr float kSkin = 1e-4f;
    constexpr float kDt = 1.0f / 60.0f;

    // Techo en y = 15
    auto isSolid = [](const glm::ivec3& p) { return p.y >= 15; };

    CharacterController controller;
    controller.setMode(CharacterMode::Fly);
    glm::vec3 feet(0.0f, 10.0f, 0.0f);

    // 1. Sin input en el aire no cae
    for (int i = 0; i < 60; ++i) {
        feet = stepCharacter(controller, feet, kDt, isSolid);
    }
    CHECK(feet.y == doctest::Approx(10.0f));
    CHECK(controller.velocity == glm::vec3(0.0f));

    // 2. moveUp acelera verticalmente hacia arriba
    controller.input.moveUp = 1.0f;
    for (int i = 0; i < 120; ++i) {
        feet = stepCharacter(controller, feet, kDt, isSolid);
    }

    // Altura máxima limitada por el techo en y = 15
    // Caja tiene altura 2 * halfExtents.y (1.8m), la cima toca 15.0 - skin
    const float expectedMaxFeetY = 15.0f - kSkin - 2.0f * controller.halfExtents.y;
    CHECK(feet.y == doctest::Approx(expectedMaxFeetY));
    CHECK(controller.velocity.y == doctest::Approx(0.0f));
}

TEST_CASE("character: toggleMode, enabled = false y dt = 0") {
    auto isSolid = [](const glm::ivec3&) { return false; };

    CharacterController controller;
    controller.velocity = glm::vec3(0.0f, -10.0f, 0.0f);

    // Al alternar a Fly, velocity.y se resetea a 0
    controller.toggleMode();
    CHECK(controller.mode == CharacterMode::Fly);
    CHECK(controller.velocity.y == doctest::Approx(0.0f));

    controller.toggleMode();
    CHECK(controller.mode == CharacterMode::Walk);

    // enabled = false no mueve la entidad
    controller.enabled = false;
    controller.input.moveForward = 1.0f;
    const glm::vec3 originalFeet(1.0f, 2.0f, 3.0f);
    glm::vec3 feet = stepCharacter(controller, originalFeet, 0.1f, isSolid);
    CHECK(feet == originalFeet);

    // dt = 0 no mueve la entidad
    controller.enabled = true;
    feet = stepCharacter(controller, originalFeet, 0.0f, isSolid);
    CHECK(feet == originalFeet);
}

TEST_CASE("character: determinismo en dos corridas de 300 pasos") {
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

TEST_CASE("character: sistemas ECS addCharacterSystem y addCharacterCameraSystem") {
    World world;
    auto player = world.registry().create();
    world.registry().add<Transform>(player, glm::vec3(0.0f, 10.0f, 0.0f));
    CharacterController ctrl;
    world.registry().add<CharacterController>(player, ctrl);

    // Sistema de física en Fixed
    addCharacterSystem(world, [](const glm::ivec3&) { return false; });
    world.run(SystemPhase::Fixed, 1.0f / 60.0f);

    const auto& playerTransform = world.registry().get<Transform>(player);
    // Debe haber descendido por gravedad
    CHECK(playerTransform.position.y < 10.0f);

    // Cámara vinculada al jugador
    auto camera = world.registry().create();
    world.registry().add<Transform>(camera);
    world.registry().add<Camera>(camera);
    world.registry().add<CharacterCamera>(camera, CharacterCamera{player});

    Time time;
    addCharacterCameraSystem(world, time);
    world.run(SystemPhase::Render, 1.0f / 60.0f);

    const auto& cameraTransform = world.registry().get<Transform>(camera);
    const auto& currentCtrl = world.registry().get<CharacterController>(player);
    // Con alpha = 0.0f en Time por defecto, la posición es previousPosition + eyeHeight
    CHECK(cameraTransform.position.y ==
          doctest::Approx(currentCtrl.previousPosition.y + currentCtrl.eyeHeight));

    // Test puro de interpolación characterEyePosition
    Transform trFinal{glm::vec3(10.0f, 20.0f, 30.0f)};
    CharacterController ctrlInterpolation;
    ctrlInterpolation.previousPosition = glm::vec3(0.0f, 10.0f, 20.0f);
    ctrlInterpolation.eyeHeight = 1.62f;

    const glm::vec3 eyeHalf = characterEyePosition(ctrlInterpolation, trFinal, 0.5f);
    CHECK(eyeHalf.x == doctest::Approx(5.0f));
    CHECK(eyeHalf.y == doctest::Approx(15.0f + 1.62f));
    CHECK(eyeHalf.z == doctest::Approx(25.0f));
}
