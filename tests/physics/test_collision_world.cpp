#include <doctest/doctest.h>

#include <mge/physics/CollisionWorld.h>
#include <mge/procgen/Primitives.h>
#include <mge/voxel/Block.h>
#include <mge/voxel/VoxelWorld.h>

#include <glm/gtc/quaternion.hpp>

using namespace mge;

TEST_CASE("CollisionWorld: trigger Enter, Stay, Exit events across 3 steps (sword vs enemy)") {
    World world;
    CollisionWorld cw;
    addCollisionWorldSystem(world, cw);

    constexpr std::uint32_t kLayerSword = 1u << 0;
    constexpr std::uint32_t kLayerEnemy = 1u << 1;

    const Entity sword = world.registry().create();
    world.registry().add<Transform>(sword, Transform{glm::vec3(0.0f, 1.0f, 0.0f)});
    Collider swordCol = Collider::box(glm::vec3(0.5f), /*isTrigger=*/true);
    swordCol.motion = ColliderMotionType::Kinematic;
    swordCol.layer = kLayerSword;
    swordCol.mask = kLayerEnemy;
    world.registry().add<Collider>(sword, swordCol);

    const Entity enemy = world.registry().create();
    world.registry().add<Transform>(enemy, Transform{glm::vec3(0.6f, 1.0f, 0.0f)});
    Collider enemyCol = Collider::capsule(0.4f, 1.8f, /*isTrigger=*/true);
    enemyCol.layer = kLayerEnemy;
    enemyCol.mask = kLayerSword;
    world.registry().add<Collider>(enemy, enemyCol);

    // Step 1: overlapping -> exactly one Enter event
    world.run(SystemPhase::Fixed, 1.0f / 60.0f);
    REQUIRE(cw.triggerEvents().size() == 1);
    CHECK(cw.triggerEvents()[0].type == TriggerEventType::Enter);
    CHECK(cw.triggerEvents()[0].a == sword);
    CHECK(cw.triggerEvents()[0].b == enemy);

    // Step 2: still overlapping -> exactly one Stay event
    world.run(SystemPhase::Fixed, 1.0f / 60.0f);
    REQUIRE(cw.triggerEvents().size() == 1);
    CHECK(cw.triggerEvents()[0].type == TriggerEventType::Stay);

    // Step 3: sword moves away -> exactly one Exit event
    world.registry().get<Transform>(sword).position = glm::vec3(5.0f, 1.0f, 0.0f);
    world.run(SystemPhase::Fixed, 1.0f / 60.0f);
    REQUIRE(cw.triggerEvents().size() == 1);
    CHECK(cw.triggerEvents()[0].type == TriggerEventType::Exit);

    // Step 4: stays separated -> no events
    world.run(SystemPhase::Fixed, 1.0f / 60.0f);
    CHECK(cw.triggerEvents().empty());
}

TEST_CASE("CollisionWorld: layer and mask filtering on raycast, overlap, and triggers") {
    CollisionWorld cw;
    constexpr std::uint32_t kLayerWall = 1u << 0;
    constexpr std::uint32_t kLayerEnemy = 1u << 1;
    constexpr std::uint32_t kLayerTrigger = 1u << 2;

    const Entity wall{1, 1};
    Collider wallCol = Collider::box(glm::vec3(1.0f));
    wallCol.layer = kLayerWall;
    wallCol.mask = kLayerWall;
    cw.upsert(wall, wallCol, Transform{glm::vec3(0.0f, 0.0f, 0.0f)});

    const Entity enemy{2, 1};
    Collider enemyCol = Collider::sphere(1.0f);
    enemyCol.layer = kLayerEnemy;
    enemyCol.mask = kLayerEnemy;
    cw.upsert(enemy, enemyCol, Transform{glm::vec3(0.0f, 0.0f, 5.0f)});

    const Entity zone{3, 1};
    Collider zoneCol = Collider::box(glm::vec3(2.0f), /*isTrigger=*/true);
    zoneCol.layer = kLayerTrigger;
    zoneCol.mask = kLayerWall; // Only interacts with wall, NOT enemy
    cw.upsert(zone, zoneCol, Transform{glm::vec3(0.0f, 0.0f, 5.0f)});

    // Trigger at z=5 overlaps enemy geometrically, but mask excludes kLayerEnemy -> no trigger event
    CHECK(cw.stepTriggers().empty());

    // Raycast from z = -5 towards +Z
    const Ray ray({0.0f, 0.0f, -5.0f}, {0.0f, 0.0f, 1.0f});
    const auto hitAll = cw.raycast(ray, 20.0f);
    REQUIRE(hitAll.has_value());
    CHECK(hitAll->entity == wall);

    // Raycast filtering only kLayerEnemy skips wall and hits enemy at z = 4 (distance 9)
    const auto hitEnemy = cw.raycast(ray, 20.0f, kLayerEnemy);
    REQUIRE(hitEnemy.has_value());
    CHECK(hitEnemy->entity == enemy);
    CHECK(hitEnemy->distance == doctest::Approx(9.0f));

    // Overlap at z = 5 with mask kLayerEnemy returns only enemy
    std::vector<Entity> found;
    cw.overlapSphere(Sphere{{0.0f, 0.0f, 5.0f}, 0.5f}, found, kLayerEnemy);
    REQUIRE(found.size() == 1);
    CHECK(found[0] == enemy);
}

TEST_CASE("CollisionWorld: deterministic trigger and overlap ordering regardless of insertion order") {
    const Entity e1{1, 1};
    const Entity e2{2, 1};
    const Entity e3{3, 1};
    const Collider trig = Collider::sphere(2.0f, /*isTrigger=*/true);
    const Transform atOrigin{glm::vec3(0.0f)};

    CollisionWorld cwA;
    cwA.upsert(e1, trig, atOrigin);
    cwA.upsert(e2, trig, atOrigin);
    cwA.upsert(e3, trig, atOrigin);
    const auto evA = cwA.stepTriggers();

    CollisionWorld cwB;
    cwB.upsert(e3, trig, atOrigin);
    cwB.upsert(e1, trig, atOrigin);
    cwB.upsert(e2, trig, atOrigin);
    const auto evB = cwB.stepTriggers();

    REQUIRE(evA.size() == 3);
    REQUIRE(evB.size() == 3);
    for (std::size_t i = 0; i < 3; ++i) {
        CHECK(evA[i].type == evB[i].type);
        CHECK(evA[i].a == evB[i].a);
        CHECK(evA[i].b == evB[i].b);
    }

    std::vector<Entity> ovA;
    std::vector<Entity> ovB;
    cwA.overlapSphere(Sphere{{0.0f, 0.0f, 0.0f}, 1.0f}, ovA);
    cwB.overlapSphere(Sphere{{0.0f, 0.0f, 0.0f}, 1.0f}, ovB);
    CHECK(ovA == ovB);
}

TEST_CASE("CollisionWorld: unified voxel world + MeshCollider + capsuleCast queries") {
    BlockRegistry reg;
    BlockDesc stone;
    stone.name = "stone";
    stone.solid = true;
    const BlockId stoneId = reg.add(stone);

    VoxelWorld voxels(reg);
    voxels.loadChunk({0, 0, 0});
    voxels.setBlock({2, 0, 2}, stoneId); // Solid voxel [2..3, 0..1, 2..3]

    const MeshData meshBox = primitives::box(glm::vec3(2.0f)); // [-1, 1]^3
    const MeshCollider meshCol = MeshCollider::fromMesh(meshBox);

    CollisionWorld cw;
    cw.setVoxelWorld(&voxels, 1u << 0);
    const Entity meshEntity{10, 1};
    Collider mc = Collider::staticMesh(&meshCol);
    mc.layer = 1u << 1;
    cw.upsert(meshEntity, mc, Transform{});

    // Ray hits voxel at x=2.5, y=1.0, z=2.5
    const auto vHit = cw.raycast(Ray({2.5f, 5.0f, 2.5f}, {0.0f, -1.0f, 0.0f}), 10.0f);
    REQUIRE(vHit.has_value());
    CHECK(vHit->hitVoxel);
    CHECK(vHit->point.y == doctest::Approx(1.0f));

    // Ray hits MeshCollider at x=0, y=1, z=0
    const auto mHit = cw.raycast(Ray({0.0f, 5.0f, 0.0f}, {0.0f, -1.0f, 0.0f}), 10.0f);
    REQUIRE(mHit.has_value());
    CHECK_FALSE(mHit->hitVoxel);
    CHECK(mHit->entity == meshEntity);
    CHECK(mHit->distance == doctest::Approx(4.0f));

    // High-speed capsuleCast stops before MeshCollider wall without tunnelling
    const Capsule cap{{0.0f, -0.5f, 6.0f}, 0.4f, 1.5f};
    const auto sweep = cw.capsuleCast(cap, glm::vec3(0.0f, 0.0f, -100.0f));
    REQUIRE(sweep.has_value());
    CHECK(sweep->entity == meshEntity);
    const float endZ = cap.feet.z + (-100.0f) * sweep->fraction;
    CHECK(endZ >= 1.0f + cap.radius - 0.02f);
}
