#include <doctest/doctest.h>

#include <mge/core/Random.h>
#include <mge/core/ecs/Registry.h>
#include <mge/core/ecs/Transform.h>
#include <mge/physics/PhysicsWorld.h>
#include <mge/physics/RigidBody.h>

#include <cmath>
#include <vector>

using namespace mge;

TEST_CASE("RigidBody: caida libre sin voxeles") {
    Registry registry;
    PhysicsWorld physics;
    physics.settings().gravity = glm::vec3(0.0f, -28.0f, 0.0f);

    const Entity entity = registry.create();
    registry.add<Transform>(entity, glm::vec3(0.0f, 100.0f, 0.0f));
    RigidBody rb;
    rb.mass = 1.0f;
    rb.linearDamping = 0.0f; // Sin amortiguación para verificar la fórmula pura
    registry.add<RigidBody>(entity, rb);

    constexpr float kDt = 1.0f / 60.0f;
    for (int i = 0; i < 60; ++i) {
        physics.step(registry, kDt);
    }

    const auto& body = registry.get<RigidBody>(entity);
    const auto& transform = registry.get<Transform>(entity);

    // v = v0 + g * t = 0 - 28 * 1 = -28
    CHECK(body.velocity.y == doctest::Approx(-28.0f).epsilon(0.02f));

    // y(t) = y0 - 0.5 * 28 * t^2 = 100 - 14 = 86
    // Con Euler semi-implícito se integra v(t+dt), por lo que se desplaza ~14.23 m
    CHECK(transform.position.y == doctest::Approx(86.0f).epsilon(0.02f));
}

TEST_CASE("RigidBody: pelota rebotando en suelo voxel") {
    Registry registry;
    // Suelo voxel en y <= 0 (superficie del suelo en y = 1.0)
    SolidQuery floorQuery = [](const glm::ivec3& p) { return p.y <= 0; };
    PhysicsWorld physics(PhysicsSettings{}, floorQuery);

    const Entity ball = registry.create();
    registry.add<Transform>(ball, glm::vec3(0.0f, 10.0f, 0.0f));
    RigidBody rb;
    rb.shape = ShapeType::Sphere;
    rb.radius = 0.5f;
    rb.mass = 1.0f;
    rb.restitution = 0.5f;
    registry.add<RigidBody>(ball, rb);

    constexpr float kDt = 1.0f / 60.0f;

    float lastPeak = 10.0f;
    float currentPeak = 0.0f;
    bool wasMovingUp = false;
    int bounceCount = 0;

    // Simular 4 segundos
    for (int step = 0; step < 240; ++step) {
        physics.step(registry, kDt);
        const auto& b = registry.get<RigidBody>(ball);
        const auto& t = registry.get<Transform>(ball);

        if (b.velocity.y > 0.0f) {
            wasMovingUp = true;
            currentPeak = std::max(currentPeak, t.position.y);
        } else if (wasMovingUp && b.velocity.y <= 0.0f) {
            // Fin de la fase ascendente: pico alcanzado
            wasMovingUp = false;
            CHECK(currentPeak < lastPeak);
            lastPeak = currentPeak;
            currentPeak = 0.0f;
            bounceCount++;
        }
    }

    CHECK(bounceCount >= 2);

    // Tras 4 segundos debe terminar en reposo y apoyada (grounded)
    const auto& finalBody = registry.get<RigidBody>(ball);
    CHECK(finalBody.grounded);
    CHECK(std::abs(finalBody.velocity.y) < 0.01f);
}

TEST_CASE("RigidBody: restitucion cero no rebota") {
    Registry registry;
    SolidQuery floorQuery = [](const glm::ivec3& p) { return p.y <= 0; };
    PhysicsWorld physics(PhysicsSettings{}, floorQuery);

    const Entity box = registry.create();
    registry.add<Transform>(box, glm::vec3(0.0f, 5.0f, 0.0f));
    RigidBody rb;
    rb.shape = ShapeType::Box;
    rb.halfExtents = glm::vec3(0.5f);
    rb.mass = 1.0f;
    rb.restitution = 0.0f;
    registry.add<RigidBody>(box, rb);

    constexpr float kDt = 1.0f / 60.0f;
    bool hitFloor = false;

    for (int step = 0; step < 120; ++step) {
        physics.step(registry, kDt);
        const auto& b = registry.get<RigidBody>(box);
        if (b.grounded) {
            hitFloor = true;
        }
        if (hitFloor) {
            // Nunca debe rebotar hacia arriba tras hacer contacto
            CHECK(b.velocity.y <= 0.001f);
            CHECK(b.grounded);
        }
    }

    CHECK(hitFloor);
}

TEST_CASE("RigidBody: caja deslizandose con friccion vs sin friccion") {
    Registry registry;
    SolidQuery floorQuery = [](const glm::ivec3& p) { return p.y <= 0; };
    PhysicsWorld physics(PhysicsSettings{}, floorQuery);

    // Caja con fricción 0.5 apoyada en el suelo
    const Entity boxWithFriction = registry.create();
    registry.add<Transform>(boxWithFriction, glm::vec3(0.0f, 1.5f, 0.0f));
    RigidBody rbA;
    rbA.halfExtents = glm::vec3(0.5f);
    rbA.mass = 1.0f;
    rbA.friction = 0.5f;
    rbA.linearDamping = 0.0f;
    rbA.velocity = glm::vec3(10.0f, 0.0f, 0.0f);
    registry.add<RigidBody>(boxWithFriction, rbA);

    // Caja sin fricción apoyada en el suelo
    const Entity boxNoFriction = registry.create();
    registry.add<Transform>(boxNoFriction, glm::vec3(0.0f, 1.5f, 5.0f));
    RigidBody rbB;
    rbB.halfExtents = glm::vec3(0.5f);
    rbB.mass = 1.0f;
    rbB.friction = 0.0f;
    rbB.linearDamping = 0.0f;
    rbB.velocity = glm::vec3(10.0f, 0.0f, 0.0f);
    registry.add<RigidBody>(boxNoFriction, rbB);

    constexpr float kDt = 1.0f / 60.0f;
    // Con fricción 0.5 y g = 28: a = 14 m/s^2. Tarda 10 / 14 ≈ 0.71 s (43 pasos) en detenerse
    for (int step = 0; step < 90; ++step) {
        physics.step(registry, kDt);
    }

    const auto& finalA = registry.get<RigidBody>(boxWithFriction);
    const auto& finalB = registry.get<RigidBody>(boxNoFriction);

    CHECK(std::abs(finalA.velocity.x) < 0.001f);
    CHECK(finalB.velocity.x == doctest::Approx(10.0f).epsilon(0.001f));
}

TEST_CASE("RigidBody: colision frontal de dos esferas con restitucion 1") {
    Registry registry;
    // Sin gravedad ni mundo voxel
    PhysicsSettings settings;
    settings.gravity = glm::vec3(0.0f);
    PhysicsWorld physics(settings);

    const Entity s1 = registry.create();
    registry.add<Transform>(s1, glm::vec3(-5.0f, 0.0f, 0.0f));
    RigidBody rb1;
    rb1.shape = ShapeType::Sphere;
    rb1.radius = 0.5f;
    rb1.mass = 1.0f;
    rb1.restitution = 1.0f;
    rb1.linearDamping = 0.0f;
    rb1.velocity = glm::vec3(6.0f, 0.0f, 0.0f);
    registry.add<RigidBody>(s1, rb1);

    const Entity s2 = registry.create();
    registry.add<Transform>(s2, glm::vec3(5.0f, 0.0f, 0.0f));
    RigidBody rb2;
    rb2.shape = ShapeType::Sphere;
    rb2.radius = 0.5f;
    rb2.mass = 1.0f;
    rb2.restitution = 1.0f;
    rb2.linearDamping = 0.0f;
    rb2.velocity = glm::vec3(-4.0f, 0.0f, 0.0f);
    registry.add<RigidBody>(s2, rb2);

    constexpr float kDt = 1.0f / 60.0f;
    for (int step = 0; step < 90; ++step) {
        physics.step(registry, kDt);
    }

    const auto& final1 = registry.get<RigidBody>(s1);
    const auto& final2 = registry.get<RigidBody>(s2);

    // Deben intercambiar velocidades dentro del 1%
    CHECK(final1.velocity.x == doctest::Approx(-4.0f).epsilon(0.01f));
    CHECK(final2.velocity.x == doctest::Approx(6.0f).epsilon(0.01f));
}

TEST_CASE("RigidBody: caja dinamica sobre caja estatica no se hunde") {
    Registry registry;
    PhysicsWorld physics;

    const Entity staticBox = registry.create();
    registry.add<Transform>(staticBox, glm::vec3(0.0f, 0.0f, 0.0f));
    RigidBody rbStatic;
    rbStatic.shape = ShapeType::Box;
    rbStatic.halfExtents = glm::vec3(1.0f);
    rbStatic.mass = 0.0f; // Estático
    registry.add<RigidBody>(staticBox, rbStatic);

    const Entity dynamicBox = registry.create();
    registry.add<Transform>(dynamicBox, glm::vec3(0.0f, 2.0f, 0.0f));
    RigidBody rbDyn;
    rbDyn.shape = ShapeType::Box;
    rbDyn.halfExtents = glm::vec3(1.0f);
    rbDyn.mass = 1.0f;
    registry.add<RigidBody>(dynamicBox, rbDyn);

    constexpr float kDt = 1.0f / 60.0f;
    for (int step = 0; step < 600; ++step) {
        physics.step(registry, kDt);
    }

    const auto& tfStatic = registry.get<Transform>(staticBox);
    const auto& tfDyn = registry.get<Transform>(dynamicBox);
    const auto& rbDynFinal = registry.get<RigidBody>(dynamicBox);

    // La caja estática no se movió
    CHECK(tfStatic.position == glm::vec3(0.0f, 0.0f, 0.0f));

    // La caja dinámica no se hunde (penetración máxima insignificante < 0.05)
    CHECK(tfDyn.position.y >= 1.95f);
    CHECK(rbDynFinal.grounded);
}

TEST_CASE("RigidBody: pila de 3 cajas estable tras 600 pasos") {
    Registry registry;
    SolidQuery floorQuery = [](const glm::ivec3& p) { return p.y <= 0; };
    PhysicsWorld physics(PhysicsSettings{}, floorQuery);

    // Suelo en y <= 0 (superficie en y = 1.0). Cajas de tamaño 1x1x1 (halfExtents = 0.5)
    const Entity b1 = registry.create();
    registry.add<Transform>(b1, glm::vec3(0.0f, 1.5f, 0.0f));
    RigidBody rb1;
    rb1.halfExtents = glm::vec3(0.5f);
    rb1.mass = 1.0f;
    registry.add<RigidBody>(b1, rb1);

    const Entity b2 = registry.create();
    registry.add<Transform>(b2, glm::vec3(0.0f, 2.5f, 0.0f));
    RigidBody rb2;
    rb2.halfExtents = glm::vec3(0.5f);
    rb2.mass = 1.0f;
    registry.add<RigidBody>(b2, rb2);

    const Entity b3 = registry.create();
    registry.add<Transform>(b3, glm::vec3(0.0f, 3.5f, 0.0f));
    RigidBody rb3;
    rb3.halfExtents = glm::vec3(0.5f);
    rb3.mass = 1.0f;
    registry.add<RigidBody>(b3, rb3);

    constexpr float kDt = 1.0f / 60.0f;
    for (int step = 0; step < 600; ++step) {
        physics.step(registry, kDt);
    }

    const auto& tf1 = registry.get<Transform>(b1);
    const auto& tf2 = registry.get<Transform>(b2);
    const auto& tf3 = registry.get<Transform>(b3);

    const float pen12 = (tf1.position.y + 0.5f) - (tf2.position.y - 0.5f);
    const float pen23 = (tf2.position.y + 0.5f) - (tf3.position.y - 0.5f);

    CHECK(pen12 >= 0.0f);
    CHECK(pen12 < 0.05f);
    CHECK(pen23 >= 0.0f);
    CHECK(pen23 < 0.05f);
}

namespace {
    float randomFloat(Pcg32& rng, float minVal, float maxVal) {
        return minVal + rng.nextFloat01() * (maxVal - minVal);
    }
} // namespace

TEST_CASE("RigidBody: determinismo con 50 cuerpos aleatorios") {
    auto runSimulation = [](std::vector<glm::vec3>& outPositions) {
        Registry registry;
        SolidQuery floorQuery = [](const glm::ivec3& p) { return p.y <= 0; };
        PhysicsWorld physics(PhysicsSettings{}, floorQuery);

        Pcg32 rng(12345u);
        std::vector<Entity> entities;
        entities.reserve(50);

        for (int i = 0; i < 50; ++i) {
            const Entity e = registry.create();
            entities.push_back(e);

            const float px = randomFloat(rng, -10.0f, 10.0f);
            const float py = randomFloat(rng, 2.0f, 20.0f);
            const float pz = randomFloat(rng, -10.0f, 10.0f);
            registry.add<Transform>(e, glm::vec3(px, py, pz));

            RigidBody rb;
            rb.shape = (i % 2 == 0) ? ShapeType::Box : ShapeType::Sphere;
            rb.halfExtents = glm::vec3(0.5f);
            rb.radius = 0.5f;
            rb.mass = randomFloat(rng, 0.5f, 5.0f);
            rb.restitution = randomFloat(rng, 0.1f, 0.8f);
            rb.friction = randomFloat(rng, 0.1f, 0.8f);
            rb.velocity = glm::vec3(randomFloat(rng, -5.0f, 5.0f), randomFloat(rng, -2.0f, 5.0f),
                                    randomFloat(rng, -5.0f, 5.0f));
            registry.add<RigidBody>(e, rb);
        }

        constexpr float kDt = 1.0f / 60.0f;
        for (int step = 0; step < 300; ++step) {
            physics.step(registry, kDt);
        }

        outPositions.clear();
        for (const Entity e : entities) {
            outPositions.push_back(registry.get<Transform>(e).position);
        }
    };

    std::vector<glm::vec3> runA;
    std::vector<glm::vec3> runB;

    runSimulation(runA);
    runSimulation(runB);

    REQUIRE(runA.size() == 50);
    REQUIRE(runB.size() == 50);

    for (std::size_t i = 0; i < 50; ++i) {
        CHECK(runA[i].x == runB[i].x);
        CHECK(runA[i].y == runB[i].y);
        CHECK(runA[i].z == runB[i].z);
    }
}

TEST_CASE("RigidBody: sistema ECS addPhysicsSystem") {
    World ecsWorld;
    PhysicsWorld physics;
    addPhysicsSystem(ecsWorld, physics);

    const Entity entity = ecsWorld.registry().create();
    ecsWorld.registry().add<Transform>(entity, glm::vec3(0.0f, 10.0f, 0.0f));
    RigidBody rb;
    rb.mass = 1.0f;
    rb.linearDamping = 0.0f;
    ecsWorld.registry().add<RigidBody>(entity, rb);

    ecsWorld.run(SystemPhase::Fixed, 1.0f / 60.0f);

    const auto& tf = ecsWorld.registry().get<Transform>(entity);
    const auto& b = ecsWorld.registry().get<RigidBody>(entity);

    CHECK(b.velocity.y < 0.0f);
    CHECK(tf.position.y < 10.0f);
}

TEST_CASE("RigidBody: sin NaN ni Inf en ningun paso") {
    Registry registry;
    SolidQuery floorQuery = [](const glm::ivec3& p) { return p.y <= 0; };
    PhysicsWorld physics(PhysicsSettings{}, floorQuery);

    Pcg32 rng(98765u);
    std::vector<Entity> entities;
    entities.reserve(30);

    for (int i = 0; i < 30; ++i) {
        const Entity e = registry.create();
        entities.push_back(e);

        const float px = randomFloat(rng, -5.0f, 5.0f);
        const float py = randomFloat(rng, 1.5f, 15.0f);
        const float pz = randomFloat(rng, -5.0f, 5.0f);
        registry.add<Transform>(e, glm::vec3(px, py, pz));

        RigidBody rb;
        rb.shape = (i % 2 == 0) ? ShapeType::Box : ShapeType::Sphere;
        rb.halfExtents = glm::vec3(0.5f);
        rb.radius = 0.5f;
        rb.mass = randomFloat(rng, 0.5f, 5.0f);
        rb.velocity = glm::vec3(randomFloat(rng, -10.0f, 10.0f), randomFloat(rng, -5.0f, 10.0f),
                                randomFloat(rng, -10.0f, 10.0f));
        registry.add<RigidBody>(e, rb);
    }

    constexpr float kDt = 1.0f / 60.0f;
    for (int step = 0; step < 300; ++step) {
        physics.step(registry, kDt);

        for (const Entity e : entities) {
            const auto& t = registry.get<Transform>(e);
            const auto& b = registry.get<RigidBody>(e);

            CHECK(!std::isnan(t.position.x));
            CHECK(!std::isnan(t.position.y));
            CHECK(!std::isnan(t.position.z));
            CHECK(!std::isinf(t.position.x));
            CHECK(!std::isinf(t.position.y));
            CHECK(!std::isinf(t.position.z));

            CHECK(!std::isnan(b.velocity.x));
            CHECK(!std::isnan(b.velocity.y));
            CHECK(!std::isnan(b.velocity.z));
            CHECK(!std::isinf(b.velocity.x));
            CHECK(!std::isinf(b.velocity.y));
            CHECK(!std::isinf(b.velocity.z));
        }
    }
}
