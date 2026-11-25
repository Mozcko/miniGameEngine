#include "VoxelTestWorld.h"

#include <doctest/doctest.h>
#include <glm/gtc/constants.hpp>
#include <mge/voxel/VoxelRaycast.h>

#include <cmath>
#include <limits>

using namespace mge;
using mge_test::TestBlocks;

TEST_CASE("raycastVoxels: hits on all 6 faces (+X, -X, +Y, -Y, +Z, -Z)") {
    TestBlocks b;
    VoxelWorld world(b.registry);
    world.loadChunk({0, 0, 0});

    const glm::ivec3 targetBlock(5, 5, 5);
    world.setBlock(targetBlock, b.stone);

    SUBCASE("From -X toward +X (hits left face)") {
        const Ray ray({2.5f, 5.5f, 5.5f}, {1.0f, 0.0f, 0.0f});
        const auto hit = raycastVoxels(world, ray, 10.0f);
        REQUIRE(hit.has_value());
        CHECK(hit->block == targetBlock);
        CHECK(hit->normal == glm::ivec3(-1, 0, 0));
        CHECK(hit->faceNormal() == glm::vec3(-1.0f, 0.0f, 0.0f));
        CHECK(hit->face == BlockFace::Left);
        CHECK(hit->blockId == b.stone);
        CHECK(hit->distance == doctest::Approx(2.5f));
        CHECK(hit->point.x == doctest::Approx(5.0f));
        CHECK(hit->point.y == doctest::Approx(5.5f));
        CHECK(hit->point.z == doctest::Approx(5.5f));
    }

    SUBCASE("From +X toward -X (hits right face)") {
        const Ray ray({7.5f, 5.5f, 5.5f}, {-1.0f, 0.0f, 0.0f});
        const auto hit = raycastVoxels(world, ray, 10.0f);
        REQUIRE(hit.has_value());
        CHECK(hit->block == targetBlock);
        CHECK(hit->normal == glm::ivec3(1, 0, 0));
        CHECK(hit->face == BlockFace::Right);
        CHECK(hit->distance == doctest::Approx(1.5f));
        CHECK(hit->point.x == doctest::Approx(6.0f));
    }

    SUBCASE("From -Y toward +Y (hits bottom face)") {
        const Ray ray({5.5f, 2.0f, 5.5f}, {0.0f, 1.0f, 0.0f});
        const auto hit = raycastVoxels(world, ray, 10.0f);
        REQUIRE(hit.has_value());
        CHECK(hit->block == targetBlock);
        CHECK(hit->normal == glm::ivec3(0, -1, 0));
        CHECK(hit->face == BlockFace::Bottom);
        CHECK(hit->distance == doctest::Approx(3.0f));
        CHECK(hit->point.y == doctest::Approx(5.0f));
    }

    SUBCASE("From +Y toward -Y (hits top face)") {
        const Ray ray({5.5f, 9.5f, 5.5f}, {0.0f, -1.0f, 0.0f});
        const auto hit = raycastVoxels(world, ray, 10.0f);
        REQUIRE(hit.has_value());
        CHECK(hit->block == targetBlock);
        CHECK(hit->normal == glm::ivec3(0, 1, 0));
        CHECK(hit->face == BlockFace::Top);
        CHECK(hit->distance == doctest::Approx(3.5f));
        CHECK(hit->point.y == doctest::Approx(6.0f));
    }

    SUBCASE("From -Z toward +Z (hits back face)") {
        const Ray ray({5.5f, 5.5f, 1.0f}, {0.0f, 0.0f, 1.0f});
        const auto hit = raycastVoxels(world, ray, 10.0f);
        REQUIRE(hit.has_value());
        CHECK(hit->block == targetBlock);
        CHECK(hit->normal == glm::ivec3(0, 0, -1));
        CHECK(hit->face == BlockFace::Back);
        CHECK(hit->distance == doctest::Approx(4.0f));
        CHECK(hit->point.z == doctest::Approx(5.0f));
    }

    SUBCASE("From +Z toward -Z (hits front face)") {
        const Ray ray({5.5f, 5.5f, 8.2f}, {0.0f, 0.0f, -1.0f});
        const auto hit = raycastVoxels(world, ray, 10.0f);
        REQUIRE(hit.has_value());
        CHECK(hit->block == targetBlock);
        CHECK(hit->normal == glm::ivec3(0, 0, 1));
        CHECK(hit->face == BlockFace::Front);
        CHECK(hit->distance == doctest::Approx(2.2f));
        CHECK(hit->point.z == doctest::Approx(6.0f));
    }
}

TEST_CASE("raycastVoxels: crosses positive and negative chunk borders") {
    TestBlocks b;
    VoxelWorld world(b.registry);

    SUBCASE("Crossing into positive neighbor chunk (+X, from chunk 0 to chunk 1)") {
        world.loadChunk({0, 0, 0});
        world.loadChunk({1, 0, 0});

        // Block at x = 18 (chunk {1, 0, 0}, local x = 2)
        const glm::ivec3 target(18, 4, 4);
        world.setBlock(target, b.stone);

        // Ray starts in chunk 0 at x = 14.5, crosses border x = 16, and reaches x = 18
        const Ray ray({14.5f, 4.5f, 4.5f}, {1.0f, 0.0f, 0.0f});
        const auto hit = raycastVoxels(world, ray, 10.0f);
        REQUIRE(hit.has_value());
        CHECK(hit->block == target);
        CHECK(hit->normal == glm::ivec3(-1, 0, 0));
        CHECK(hit->distance == doctest::Approx(3.5f));
    }

    SUBCASE("Crossing into negative neighbor chunk (-X, from chunk 0 to chunk -1)") {
        world.loadChunk({0, 0, 0});
        world.loadChunk({-1, 0, 0});

        // Block at x = -3 (chunk {-1, 0, 0}, local x = 13)
        const glm::ivec3 target(-3, 4, 4);
        world.setBlock(target, b.stone);

        // Ray starts in chunk 0 at x = 1.5, crosses border x = 0, and reaches x = -2
        const Ray ray({1.5f, 4.5f, 4.5f}, {-1.0f, 0.0f, 0.0f});
        const auto hit = raycastVoxels(world, ray, 10.0f);
        REQUIRE(hit.has_value());
        CHECK(hit->block == target);
        CHECK(hit->normal == glm::ivec3(1, 0, 0));
        CHECK(hit->distance == doctest::Approx(3.5f));
    }

    SUBCASE("Diagonal crossing through negative chunks in X, Y, Z") {
        world.loadChunk({0, 0, 0});
        world.loadChunk({-1, -1, -1});

        const glm::ivec3 target(-2, -2, -2);
        world.setBlock(target, b.stone);

        const glm::vec3 start(1.5f, 1.5f, 1.5f);
        const Ray ray(start, glm::vec3(-1.0f)); // Normalized in Ray ctor
        const auto hit = raycastVoxels(world, ray, 20.0f);
        REQUIRE(hit.has_value());
        CHECK(hit->block == target);
        // Reaches the outer top-right corner (-1, -1, -1) of the target block
        CHECK(hit->distance > 0.0f);
    }
}

TEST_CASE("raycastVoxels: origin inside solid block reports hit at t = 0") {
    TestBlocks b;
    VoxelWorld world(b.registry);
    world.loadChunk({0, 0, 0});

    const glm::ivec3 target(3, 3, 3);
    world.setBlock(target, b.stone);

    const Ray ray({3.4f, 3.8f, 3.1f}, {0.3f, 0.9f, 0.1f});
    const auto hit = raycastVoxels(world, ray, 5.0f);
    REQUIRE(hit.has_value());
    CHECK(hit->block == target);
    CHECK(hit->distance == 0.0f);
    CHECK(hit->normal == glm::ivec3(0));
    CHECK(hit->blockId == b.stone);
    CHECK(hit->point == ray.origin);
}

TEST_CASE("raycastVoxels: ignores non-solid blocks (water and flowers) and hits solid") {
    TestBlocks b;
    VoxelWorld world(b.registry);
    world.loadChunk({0, 0, 0});

    world.setBlock({2, 1, 1}, b.water);
    world.setBlock({3, 1, 1}, b.flower);
    world.setBlock({4, 1, 1}, b.stone);

    const Ray ray({0.5f, 1.5f, 1.5f}, {1.0f, 0.0f, 0.0f});
    const auto hit = raycastVoxels(world, ray, 10.0f);
    REQUIRE(hit.has_value());
    CHECK(hit->block == glm::ivec3(4, 1, 1));
    CHECK(hit->distance == doctest::Approx(3.5f));
    CHECK(hit->normal == glm::ivec3(-1, 0, 0));
}

TEST_CASE("raycastVoxels: maxDistance limits ray reach") {
    TestBlocks b;
    VoxelWorld world(b.registry);
    world.loadChunk({0, 0, 0});

    world.setBlock({8, 0, 0}, b.stone);
    const Ray ray({0.5f, 0.5f, 0.5f}, {1.0f, 0.0f, 0.0f});

    CHECK_FALSE(raycastVoxels(world, ray, 5.0f).has_value());

    const auto hit = raycastVoxels(world, ray, 8.0f);
    REQUIRE(hit.has_value());
    CHECK(hit->block == glm::ivec3(8, 0, 0));
    CHECK(hit->distance == doctest::Approx(7.5f));
}

TEST_CASE("raycastVoxels: empty world or ray into empty space returns nullopt") {
    TestBlocks b;
    VoxelWorld world(b.registry);
    world.loadChunk({0, 0, 0});

    const Ray ray({0.5f, 0.5f, 0.5f}, {0.0f, 1.0f, 0.0f});
    CHECK_FALSE(raycastVoxels(world, ray, 50.0f).has_value());
}

TEST_CASE("raycastVoxels: invalid parameters return nullopt") {
    TestBlocks b;
    VoxelWorld world(b.registry);
    world.loadChunk({0, 0, 0});
    world.setBlock({2, 0, 0}, b.stone);

    const Ray ray({0.5f, 0.5f, 0.5f}, {1.0f, 0.0f, 0.0f});

    CHECK_FALSE(raycastVoxels(world, ray, -1.0f).has_value());
    CHECK_FALSE(raycastVoxels(world, ray, std::numeric_limits<float>::quiet_NaN()).has_value());
    CHECK_FALSE(raycastVoxels(world, ray, 0.0f).has_value());
}

TEST_CASE("raycastVoxels: deterministic tie-breaking on corners/edges prefers X over Y over Z") {
    TestBlocks b;
    VoxelWorld world(b.registry);
    world.loadChunk({0, 0, 0});
    world.loadChunk({1, 0, 0});

    // Solid blocks at (1, 0, 0) and (1, 1, 0)
    world.setBlock({1, 0, 0}, b.stone);
    world.setBlock({1, 1, 0}, b.stone);

    // Ray from center of (0,0,0) along exact diagonal (1, 1, 0) toward the edge
    const Ray ray({0.5f, 0.5f, 0.5f}, {1.0f, 1.0f, 0.0f});
    const auto hit = raycastVoxels(world, ray, 10.0f);
    REQUIRE(hit.has_value());
    // Tie-breaking should choose X first, hitting block (1, 0, 0) on its left face (-X)
    CHECK(hit->block == glm::ivec3(1, 0, 0));
    CHECK(hit->normal == glm::ivec3(-1, 0, 0));
    CHECK(hit->face == BlockFace::Left);
}

TEST_CASE("raycastVoxels: infinite maxDistance does not crash and finds block at distance 50") {
    TestBlocks b;
    VoxelWorld world(b.registry);
    for (int cx = 0; cx <= 3; ++cx) {
        world.loadChunk({cx, 0, 0});
    }

    world.setBlock({50, 0, 0}, b.stone);
    const Ray ray({0.5f, 0.5f, 0.5f}, {1.0f, 0.0f, 0.0f});

    const auto hit = raycastVoxels(world, ray, std::numeric_limits<float>::infinity());
    REQUIRE(hit.has_value());
    CHECK(hit->block == glm::ivec3(50, 0, 0));
    CHECK(hit->distance == doctest::Approx(49.5f));
}

TEST_CASE("raycastVoxels: overload with pickable predicate selects non-solid blocks") {
    TestBlocks b;
    VoxelWorld world(b.registry);
    world.loadChunk({0, 0, 0});

    const glm::ivec3 flowerBlock(5, 5, 5);
    world.setBlock(flowerBlock, b.flower);

    const Ray ray({0.5f, 5.5f, 5.5f}, {1.0f, 0.0f, 0.0f});

    // Standard overload (solids only) ignores the non-solid flower
    CHECK_FALSE(raycastVoxels(world, ray, 10.0f).has_value());

    // With a custom predicate that accepts the flower, it hits as expected
    const auto hit = raycastVoxels(world, ray, 10.0f, [&](BlockId id) { return id == b.flower; });
    REQUIRE(hit.has_value());
    CHECK(hit->block == flowerBlock);
    CHECK(hit->blockId == b.flower);
    CHECK(hit->normal == glm::ivec3(-1, 0, 0));
    CHECK(hit->distance == doctest::Approx(4.5f));
}

TEST_CASE("raycastVoxels: sandbox ignores water and selects non-solid block behind it") {
    TestBlocks b;
    VoxelWorld world(b.registry);
    world.loadChunk({0, 0, 0});

    world.setBlock({3, 5, 5}, b.water);
    world.setBlock({6, 5, 5}, b.flower);

    const Ray ray({0.5f, 5.5f, 5.5f}, {1.0f, 0.0f, 0.0f});

    // Sandbox-style predicate: everything non-air except water
    auto sandboxPickable = [&](BlockId id) { return id != kAir && id != b.water; };

    const auto hit = raycastVoxels(world, ray, 10.0f, sandboxPickable);
    REQUIRE(hit.has_value());
    CHECK(hit->block == glm::ivec3(6, 5, 5));
    CHECK(hit->blockId == b.flower);
    CHECK(hit->distance == doctest::Approx(5.5f));
}

TEST_CASE("raycastVoxels: origin inside pickable non-solid block returns t = 0") {
    TestBlocks b;
    VoxelWorld world(b.registry);
    world.loadChunk({0, 0, 0});

    world.setBlock({2, 2, 2}, b.flower);
    const Ray ray({2.5f, 2.5f, 2.5f}, {0.0f, 1.0f, 0.0f});

    auto pickable = [&](BlockId id) { return id == b.flower; };
    const auto hit = raycastVoxels(world, ray, 10.0f, pickable);
    REQUIRE(hit.has_value());
    CHECK(hit->block == glm::ivec3(2, 2, 2));
    CHECK(hit->distance == 0.0f);
    CHECK(hit->normal == glm::ivec3(0));
    CHECK(hit->blockId == b.flower);
}
