#include <doctest/doctest.h>

#include <mge/core/Random.h>
#include <mge/core/ecs/Registry.h>
#include <mge/core/ecs/Transform.h>
#include <mge/physics/PhysicsWorld.h>
#include <mge/physics/RigidBody.h>

#include <cmath>
#include <vector>

using namespace mge;

TEST_CASE("RigidBody: free fall without voxels") {
    Registry registry;
    PhysicsWorld physics;
    physics.settings().gravity = glm::vec3(0.0f, -28.0f, 0.0f);

    const Entity entity = registry.create();
    registry.add<Transform>(entity, glm::vec3(0.0f, 100.0f, 0.0f));
    RigidBody rb;
    rb.mass = 1.0f;
    rb.linearDamping = 0.0f; // No damping to verify the exact formula
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
    // Semi-implicit Euler integrates v(t+dt), so displacement is ~14.23 m
    CHECK(transform.position.y == doctest::Approx(86.0f).epsilon(0.02f));
}

TEST_CASE("RigidBody: ball bouncing on voxel floor") {
    Registry registry;
    // Voxel floor at y <= 0 (floor surface at y = 1.0)
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

    // Simulate 4 seconds
    for (int step = 0; step < 240; ++step) {
        physics.step(registry, kDt);
        const auto& b = registry.get<RigidBody>(ball);
        const auto& t = registry.get<Transform>(ball);

        if (b.velocity.y > 0.0f) {
            wasMovingUp = true;
            currentPeak = std::max(currentPeak, t.position.y);
        } else if (wasMovingUp && b.velocity.y <= 0.0f) {
            // End of ascending phase: peak reached
            wasMovingUp = false;
            CHECK(currentPeak < lastPeak);
            lastPeak = currentPeak;
            currentPeak = 0.0f;
            bounceCount++;
        }
    }

    CHECK(bounceCount >= 2);

    // After 4 seconds it should come to rest and be grounded
    const auto& finalBody = registry.get<RigidBody>(ball);
    CHECK(finalBody.grounded);
    CHECK(std::abs(finalBody.velocity.y) < 0.01f);
}

TEST_CASE("RigidBody: zero restitution does not bounce") {
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
            // Must never bounce upward after making contact
            CHECK(b.velocity.y <= 0.001f);
            CHECK(b.grounded);
        }
    }

    CHECK(hitFloor);
}

TEST_CASE("RigidBody: box sliding with friction vs without friction") {
    Registry registry;
    SolidQuery floorQuery = [](const glm::ivec3& p) { return p.y <= 0; };
    PhysicsWorld physics(PhysicsSettings{}, floorQuery);

    // Box with friction 0.5 resting on the floor
    const Entity boxWithFriction = registry.create();
    registry.add<Transform>(boxWithFriction, glm::vec3(0.0f, 1.5f, 0.0f));
    RigidBody rbA;
    rbA.halfExtents = glm::vec3(0.5f);
    rbA.mass = 1.0f;
    rbA.friction = 0.5f;
    rbA.linearDamping = 0.0f;
    rbA.velocity = glm::vec3(10.0f, 0.0f, 0.0f);
    registry.add<RigidBody>(boxWithFriction, rbA);

    // Frictionless box resting on the floor
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
    // With friction 0.5 and g = 28: a = 14 m/s^2. Takes 10 / 14 ~= 0.71 s (43 steps) to stop
    for (int step = 0; step < 90; ++step) {
        physics.step(registry, kDt);
    }

    const auto& finalA = registry.get<RigidBody>(boxWithFriction);
    const auto& finalB = registry.get<RigidBody>(boxNoFriction);

    CHECK(std::abs(finalA.velocity.x) < 0.001f);
    CHECK(finalB.velocity.x == doctest::Approx(10.0f).epsilon(0.001f));
}

TEST_CASE("RigidBody: head-on collision of two spheres with restitution 1") {
    Registry registry;
    // No gravity and no voxel world
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

    // Should exchange velocities within 1%
    CHECK(final1.velocity.x == doctest::Approx(-4.0f).epsilon(0.01f));
    CHECK(final2.velocity.x == doctest::Approx(6.0f).epsilon(0.01f));
}

TEST_CASE("RigidBody: dynamic box on static box does not sink") {
    Registry registry;
    PhysicsWorld physics;

    const Entity staticBox = registry.create();
    registry.add<Transform>(staticBox, glm::vec3(0.0f, 0.0f, 0.0f));
    RigidBody rbStatic;
    rbStatic.shape = ShapeType::Box;
    rbStatic.halfExtents = glm::vec3(1.0f);
    rbStatic.mass = 0.0f; // Static
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

    // Static box did not move
    CHECK(tfStatic.position == glm::vec3(0.0f, 0.0f, 0.0f));

    // Dynamic box does not sink (negligible max penetration < 0.05)
    CHECK(tfDyn.position.y >= 1.95f);
    CHECK(rbDynFinal.grounded);
}

TEST_CASE("RigidBody: stack of 3 boxes remains stable after 600 steps") {
    Registry registry;
    SolidQuery floorQuery = [](const glm::ivec3& p) { return p.y <= 0; };
    PhysicsWorld physics(PhysicsSettings{}, floorQuery);

    // Floor at y <= 0 (surface at y = 1.0). 1x1x1 boxes (halfExtents = 0.5)
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

TEST_CASE("RigidBody: determinism with 50 random bodies") {
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

TEST_CASE("RigidBody: ECS system addPhysicsSystem") {
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

TEST_CASE("RigidBody: no NaN or Inf at any step") {
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

TEST_CASE("RigidBody: friction does not accelerate separating bodies (energy conservation)") {
    Registry registry;
    PhysicsWorld physics;
    physics.settings().gravity = glm::vec3(0.0f);
    physics.settings().solverIterations = 1;

    // Static box at the origin
    const Entity staticBox = registry.create();
    registry.add<Transform>(staticBox, glm::vec3(0.0f, 0.0f, 0.0f));
    RigidBody rbStatic;
    rbStatic.mass = 0.0f;
    rbStatic.friction = 1.0f;
    rbStatic.linearDamping = 0.0f;
    registry.add<RigidBody>(staticBox, rbStatic);

    // Dynamic box at (0, 0.95, 0) moving away with velocity (5, 2, 0)
    const Entity dynamicBox = registry.create();
    registry.add<Transform>(dynamicBox, glm::vec3(0.0f, 0.95f, 0.0f));
    RigidBody rbDynamic;
    rbDynamic.mass = 1.0f;
    rbDynamic.friction = 1.0f;
    rbDynamic.linearDamping = 0.0f;
    rbDynamic.velocity = glm::vec3(5.0f, 2.0f, 0.0f);
    registry.add<RigidBody>(dynamicBox, rbDynamic);

    constexpr float kDt = 1.0f / 60.0f;
    physics.step(registry, kDt);

    const auto& b = registry.get<RigidBody>(dynamicBox);
    CHECK(b.velocity.x <= 5.0f);
    CHECK(b.velocity.x >= 0.0f);
}

TEST_CASE("RigidBody: positional correction does not tunnel through voxel walls") {
    Registry registry;
    SolidQuery wallQuery = [](const glm::ivec3& p) { return p.x == 2; };
    PhysicsWorld physics(PhysicsSettings{}, wallQuery);
    physics.settings().gravity = glm::vec3(0.0f);

    // Static box at (1.0, 0.5, 0.5) with halfExtents 0.45
    const Entity staticBox = registry.create();
    registry.add<Transform>(staticBox, glm::vec3(1.0f, 0.5f, 0.5f));
    RigidBody rbStatic;
    rbStatic.shape = ShapeType::Box;
    rbStatic.halfExtents = glm::vec3(0.45f);
    rbStatic.mass = 0.0f;
    registry.add<RigidBody>(staticBox, rbStatic);

    // Dynamic box at (1.54, 0.5, 0.5) with halfExtents 0.45
    const Entity dynamicBox = registry.create();
    registry.add<Transform>(dynamicBox, glm::vec3(1.54f, 0.5f, 0.5f));
    RigidBody rbDynamic;
    rbDynamic.shape = ShapeType::Box;
    rbDynamic.halfExtents = glm::vec3(0.45f);
    rbDynamic.mass = 1.0f;
    registry.add<RigidBody>(dynamicBox, rbDynamic);

    constexpr float kDt = 1.0f / 60.0f;
    for (int step = 0; step < 120; ++step) {
        physics.step(registry, kDt);
    }

    const auto& tfDynamic = registry.get<Transform>(dynamicBox);
    const auto& rb = registry.get<RigidBody>(dynamicBox);
    const Aabb bounds = rb.bounds(tfDynamic.position);
    CHECK(bounds.max.x <= 2.0f);
}

// Behavior: also passes with the previous solver (gravity already gave jn > 0 at rest);
// guards the new accumulated-impulse solver against regressions.
TEST_CASE("RigidBody: resting contact friction stops box on static box") {
    Registry registry;
    PhysicsWorld physics;

    const Entity staticBox = registry.create();
    registry.add<Transform>(staticBox, glm::vec3(0.0f, 0.0f, 0.0f));
    RigidBody rbStatic;
    rbStatic.shape = ShapeType::Box;
    rbStatic.halfExtents = glm::vec3(5.0f, 0.5f, 5.0f);
    rbStatic.mass = 0.0f;
    rbStatic.friction = 0.5f;
    registry.add<RigidBody>(staticBox, rbStatic);

    const Entity dynamicBox = registry.create();
    registry.add<Transform>(dynamicBox, glm::vec3(0.0f, 1.0f, 0.0f));
    RigidBody rbDyn;
    rbDyn.shape = ShapeType::Box;
    rbDyn.halfExtents = glm::vec3(0.5f);
    rbDyn.mass = 1.0f;
    rbDyn.friction = 0.5f;
    rbDyn.linearDamping = 0.0f;
    rbDyn.velocity = glm::vec3(1.0f, 0.0f, 0.0f);
    registry.add<RigidBody>(dynamicBox, rbDyn);

    constexpr float kDt = 1.0f / 60.0f;
    for (int step = 0; step < 120; ++step) {
        physics.step(registry, kDt);
    }

    const auto& finalTf = registry.get<Transform>(dynamicBox);
    const auto& finalRb = registry.get<RigidBody>(dynamicBox);
    CHECK(glm::length(finalRb.velocity) < 0.01f);
    CHECK(finalTf.position.x < 0.5f);
    CHECK(finalTf.position.x > 0.0f);
}

TEST_CASE("RigidBody: accumulated friction stops resting box hit laterally in the same step") {
    Registry registry;
    PhysicsWorld physics;

    // Static floor (entity 0): pair (0, 1) is solved before (1, 2) on each iteration
    const Entity staticFloor = registry.create();
    registry.add<Transform>(staticFloor, glm::vec3(0.0f, 0.0f, 0.0f));
    RigidBody rbFloor;
    rbFloor.shape = ShapeType::Box;
    rbFloor.halfExtents = glm::vec3(5.0f, 0.5f, 5.0f);
    rbFloor.mass = 0.0f;
    rbFloor.friction = 1.0f;
    registry.add<RigidBody>(staticFloor, rbFloor);

    // Box resting on the floor (entity 1): friction budget = mu * g * dt = 28/60 ~= 0.467
    const Entity restingBox = registry.create();
    registry.add<Transform>(restingBox, glm::vec3(0.0f, 1.0f, 0.0f));
    RigidBody rbResting;
    rbResting.shape = ShapeType::Box;
    rbResting.halfExtents = glm::vec3(0.5f);
    rbResting.mass = 1.0f;
    rbResting.friction = 1.0f;
    rbResting.restitution = 0.0f;
    rbResting.linearDamping = 0.0f;
    registry.add<RigidBody>(restingBox, rbResting);

    // Striker box (entity 2) hits restingBox laterally with vx = 0.4 (< 0.467)
    const Entity striker = registry.create();
    registry.add<Transform>(striker, glm::vec3(-0.98f, 1.0f, 0.0f));
    RigidBody rbStriker;
    rbStriker.shape = ShapeType::Box;
    rbStriker.halfExtents = glm::vec3(0.5f);
    rbStriker.mass = 1.0f;
    rbStriker.friction = 0.0f;
    rbStriker.restitution = 0.0f;
    rbStriker.linearDamping = 0.0f;
    rbStriker.velocity = glm::vec3(0.4f, 0.0f, 0.0f);
    registry.add<RigidBody>(striker, rbStriker);

    physics.step(registry, 1.0f / 60.0f);

    // Without accumulated impulses, pair (0, 1) has jn = 0 on iterations 1..3 and restingBox stays at vx = 0.2.
    // With accumulated impulses, floor friction absorbs the push on subsequent iterations.
    const auto& finalResting = registry.get<RigidBody>(restingBox);
    CHECK(finalResting.velocity.x < 0.05f);
}

TEST_CASE("RigidBody: box on static box with zero friction keeps sliding") {
    Registry registry;
    PhysicsWorld physics;

    const Entity staticBox = registry.create();
    registry.add<Transform>(staticBox, glm::vec3(0.0f, 0.0f, 0.0f));
    RigidBody rbStatic;
    rbStatic.shape = ShapeType::Box;
    rbStatic.halfExtents = glm::vec3(5.0f, 0.5f, 5.0f);
    rbStatic.mass = 0.0f;
    rbStatic.friction = 0.0f;
    registry.add<RigidBody>(staticBox, rbStatic);

    const Entity dynamicBox = registry.create();
    registry.add<Transform>(dynamicBox, glm::vec3(0.0f, 1.0f, 0.0f));
    RigidBody rbDyn;
    rbDyn.shape = ShapeType::Box;
    rbDyn.halfExtents = glm::vec3(0.5f);
    rbDyn.mass = 1.0f;
    rbDyn.friction = 0.0f;
    rbDyn.linearDamping = 0.0f;
    rbDyn.velocity = glm::vec3(1.0f, 0.0f, 0.0f);
    registry.add<RigidBody>(dynamicBox, rbDyn);

    constexpr float kDt = 1.0f / 60.0f;
    for (int step = 0; step < 120; ++step) {
        physics.step(registry, kDt);
    }

    const auto& finalTf = registry.get<Transform>(dynamicBox);
    const auto& finalRb = registry.get<RigidBody>(dynamicBox);
    CHECK(finalRb.velocity.x == doctest::Approx(1.0f).epsilon(0.01f));
    CHECK(finalTf.position.x > 1.5f);
}

TEST_CASE("RigidBody: sphere on the edge of a solid block rolls off and falls") {
    Registry registry;
    // Single solid block at (0, 0, 0) -> occupies [0, 1]^3 (top face at y = 1.0, edge at x = 1.0)
    SolidQuery singleBlock = [](const glm::ivec3& p) { return p.x == 0 && p.y == 0 && p.z == 0; };
    PhysicsWorld physics(PhysicsSettings{}, singleBlock);

    const Entity sphere = registry.create();
    // Center 0.1 past the block edge (x = 1.1), radius 0.4, resting at top height (y = 1.4)
    registry.add<Transform>(sphere, glm::vec3(1.1f, 1.4f, 0.5f));
    RigidBody rb;
    rb.shape = ShapeType::Sphere;
    rb.radius = 0.4f;
    rb.mass = 1.0f;
    rb.friction = 0.5f;
    registry.add<RigidBody>(sphere, rb);

    constexpr float kDt = 1.0f / 60.0f;
    for (int step = 0; step < 120; ++step) {
        physics.step(registry, kDt);
    }

    const auto& finalTf = registry.get<Transform>(sphere);
    // Should fall below the top of the block (y = 1.0)
    CHECK(finalTf.position.y < 1.0f);
    CHECK(finalTf.position.x > 1.1f);
}

TEST_CASE("RigidBody: sphere centered on flat floor stays still without sinking") {
    Registry registry;
    SolidQuery floorQuery = [](const glm::ivec3& p) { return p.y <= 0; };
    PhysicsWorld physics(PhysicsSettings{}, floorQuery);

    const Entity sphere = registry.create();
    const glm::vec3 startPos(0.5f, 1.4f, 0.5f);
    registry.add<Transform>(sphere, startPos);
    RigidBody rb;
    rb.shape = ShapeType::Sphere;
    rb.radius = 0.4f;
    rb.mass = 1.0f;
    rb.friction = 0.5f;
    registry.add<RigidBody>(sphere, rb);

    constexpr float kDt = 1.0f / 60.0f;
    for (int step = 0; step < 120; ++step) {
        physics.step(registry, kDt);
    }

    const auto& finalTf = registry.get<Transform>(sphere);
    const auto& finalRb = registry.get<RigidBody>(sphere);
    CHECK(std::abs(finalTf.position.x - startPos.x) < 1e-4f);
    CHECK(std::abs(finalTf.position.z - startPos.z) < 1e-4f);
    CHECK(std::abs(finalTf.position.y - startPos.y) <= 1e-3f);
    CHECK(finalRb.grounded);
}
