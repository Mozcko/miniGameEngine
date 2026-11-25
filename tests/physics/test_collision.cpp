#include <doctest/doctest.h>
#include <mge/physics/GridQuery.h>
#include <mge/physics/VoxelCollision.h>
#include <mge/voxel/VoxelWorld.h>

#include <thread>
#include <unordered_set>
#include <vector>

using namespace mge;

namespace {

    // Minimal test blocks for VoxelWorld
    struct TestBlockRegistry {
        BlockRegistry registry;
        BlockId stone = 0;

        TestBlockRegistry() {
            BlockDesc desc;
            desc.name = "stone";
            desc.solid = true;
            stone = registry.add(desc);
        }
    };

    // Hash for querying a local set of solid blocks in tests without instantiating VoxelWorld
    struct Ivec3Hash {
        std::size_t operator()(const glm::ivec3& p) const {
            std::size_t h = std::hash<int>()(p.x);
            h ^= std::hash<int>()(p.y) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<int>()(p.z) + 0x9e3779b9 + (h << 6) + (h >> 2);
            return h;
        }
    };

} // namespace

TEST_CASE("moveAabb: falling box stops at 1.0 + skin on the floor and becomes grounded") {
    // Floor at y = 0 (block occupies y in [0, 1), so top face is at y = 1.0)
    auto isSolid = [](const glm::ivec3& b) { return b.y == 0; };

    // 0.6 x 1.8 x 0.6 box falling from y = 5.0
    Aabb box({-0.3f, 5.0f, -0.3f}, {0.3f, 6.8f, 0.3f});
    MoveOptions opts;
    opts.skin = 1e-4f;

    const MoveResult res = moveAabb(box, {0.0f, -10.0f, 0.0f}, isSolid, opts);

    CHECK(res.grounded);
    CHECK(res.blocked.y);
    CHECK_FALSE(res.blocked.x);
    CHECK_FALSE(res.blocked.z);
    CHECK(res.box.min.y == doctest::Approx(1.0f + opts.skin));
    CHECK(res.box.max.y == doctest::Approx(2.8f + opts.skin));
    CHECK(res.moved.y == doctest::Approx(1.0f + opts.skin - 5.0f));
}

TEST_CASE("moveAabb: no tunneling with huge delta (+50) against a 1-block wall") {
    // 1-block-thick wall at x = 10 (occupies [10, 11))
    auto isSolid = [](const glm::ivec3& b) { return b.x == 10; };

    Aabb box({0.0f, 0.0f, 0.0f}, {0.6f, 1.8f, 0.6f});
    MoveOptions opts;
    opts.skin = 1e-4f;

    const MoveResult res = moveAabb(box, {50.0f, 0.0f, 0.0f}, isSolid, opts);

    CHECK(res.blocked.x);
    CHECK_FALSE(res.blocked.y);
    CHECK_FALSE(res.blocked.z);
    CHECK(res.box.max.x == doctest::Approx(10.0f - opts.skin));
    CHECK(res.box.min.x == doctest::Approx(10.0f - opts.skin - 0.6f));
    CHECK(res.moved.x == doctest::Approx(10.0f - opts.skin - 0.6f));
}

TEST_CASE("moveAabb: wall sliding (diagonal motion preserves unobstructed axis)") {
    // Wall along x = 5 (blocks with x >= 5 are solid)
    auto isSolid = [](const glm::ivec3& b) { return b.x >= 5; };

    Aabb box({0.0f, 0.0f, 0.0f}, {0.6f, 1.8f, 0.6f});
    MoveOptions opts;
    opts.skin = 1e-4f;

    // Diagonal displacement toward +X and +Z
    const MoveResult res = moveAabb(box, {10.0f, 0.0f, 3.0f}, isSolid, opts);

    CHECK(res.blocked.x);
    CHECK_FALSE(res.blocked.z);
    CHECK(res.box.max.x == doctest::Approx(5.0f - opts.skin));
    CHECK(res.moved.z == doctest::Approx(3.0f));
    CHECK(res.box.min.z == doctest::Approx(3.0f));
}

TEST_CASE("moveAabb: box in exact wall contact does not move backward and can move away") {
    auto isSolid = [](const glm::ivec3& b) { return b.x >= 10; };

    // Box exactly touching the wall at max.x = 10.0
    Aabb box({9.4f, 0.0f, 0.0f}, {10.0f, 1.8f, 0.6f});
    MoveOptions opts;
    opts.skin = 1e-4f;

    SUBCASE("Moving into the wall neither advances nor retreats") {
        const MoveResult res = moveAabb(box, {1.0f, 0.0f, 0.0f}, isSolid, opts);
        CHECK(res.blocked.x);
        CHECK(res.moved.x == 0.0f);
        CHECK(res.box.max.x == doctest::Approx(10.0f));
    }

    SUBCASE("Moving away from the wall is completely unobstructed") {
        const MoveResult res = moveAabb(box, {-1.0f, 0.0f, 0.0f}, isSolid, opts);
        CHECK_FALSE(res.blocked.x);
        CHECK(res.moved.x == doctest::Approx(-1.0f));
        CHECK(res.box.max.x == doctest::Approx(9.0f));
    }
}

TEST_CASE("moveAabb: box starting inside a solid block can exit") {
    // Only block (0, 0, 0) is solid
    auto isSolid = [](const glm::ivec3& b) { return b == glm::ivec3(0, 0, 0); };

    // Box placed inside [0, 1)^3
    Aabb box({0.2f, 0.2f, 0.2f}, {0.8f, 0.8f, 0.8f});
    MoveOptions opts;
    opts.skin = 1e-4f;

    const MoveResult res = moveAabb(box, {2.0f, 0.0f, 0.0f}, isSolid, opts);

    CHECK_FALSE(res.blocked.x);
    CHECK(res.moved.x == doctest::Approx(2.0f));
    CHECK(res.box.min.x == doctest::Approx(2.2f));
}

TEST_CASE("moveAabb: collision at negative coordinates") {
    // Wall at x = -20 (block occupies [-20, -19), east face at -19.0)
    auto isSolid = [](const glm::ivec3& b) { return b.x == -20; };

    Aabb box({-15.0f, 0.0f, 0.0f}, {-14.4f, 1.8f, 0.6f});
    MoveOptions opts;
    opts.skin = 1e-4f;

    const MoveResult res = moveAabb(box, {-10.0f, 0.0f, 0.0f}, isSolid, opts);

    CHECK(res.blocked.x);
    CHECK(res.box.min.x == doctest::Approx(-19.0f + opts.skin));
    CHECK(res.moved.x == doctest::Approx(-19.0f + opts.skin - (-15.0f)));
}

TEST_CASE("moveAabb: step-up over a ledge") {
    // Floor at y = 0 (top at 1.0) for x in [0..4]
    // Step at x = 2: block (2, 1, 0) is solid (top at 2.0)
    std::unordered_set<glm::ivec3, Ivec3Hash> solids;
    for (int x = 0; x <= 5; ++x) {
        solids.insert({x, 0, 0});
    }
    solids.insert({2, 1, 0}); // 1 block tall

    auto isSolid = [&solids](const glm::ivec3& b) { return solids.count(b) != 0; };

    MoveOptions opts;
    opts.skin = 1e-4f;

    // Box resting on the floor at x = 1 (max.x = 1.6, min.y = 1.0 + skin)
    const Aabb groundedBox({1.0f, 1.0f + opts.skin, 0.0f}, {1.6f, 2.8f + opts.skin, 0.6f});

    SUBCASE("stepHeight 1.0 climbs a 1-block step") {
        opts.stepHeight = 1.0f;
        const MoveResult res = moveAabb(groundedBox, {1.0f, 0.0f, 0.0f}, isSolid, opts);

        CHECK(res.stepped);
        CHECK(res.grounded);
        // Should end up on top of the step: min.y ~= 2.0 + skin
        CHECK(res.box.min.y == doctest::Approx(2.0f + opts.skin));
        CHECK(res.box.min.x > 1.0f);
    }

    SUBCASE("2-block wall cannot be climbed with stepHeight 1.0") {
        solids.insert({2, 2, 0}); // Wall is now 2 blocks tall
        opts.stepHeight = 1.0f;
        const MoveResult res = moveAabb(groundedBox, {1.0f, 0.0f, 0.0f}, isSolid, opts);

        CHECK_FALSE(res.stepped);
        CHECK(res.blocked.x);
        CHECK(res.box.max.x == doctest::Approx(2.0f - opts.skin));
    }

    SUBCASE("stepHeight 0.6 or 0 does not climb a full 1.0 block") {
        opts.stepHeight = 0.6f;
        MoveResult res = moveAabb(groundedBox, {1.0f, 0.0f, 0.0f}, isSolid, opts);
        CHECK_FALSE(res.stepped);
        CHECK(res.blocked.x);

        opts.stepHeight = 0.0f;
        res = moveAabb(groundedBox, {1.0f, 0.0f, 0.0f}, isSolid, opts);
        CHECK_FALSE(res.stepped);
        CHECK(res.blocked.x);
    }
}

TEST_CASE("moveAabb: VoxelWorld overload (crosses chunk border and unloaded chunks are air)") {
    TestBlockRegistry reg;
    VoxelWorld world(reg.registry);

    // Load chunk {0, 0, 0} and chunk {1, 0, 0}
    world.loadChunk({0, 0, 0});
    world.loadChunk({1, 0, 0});

    // Wall exactly at x = 16 (first block of chunk {1, 0, 0})
    world.setBlock({16, 2, 0}, reg.stone);

    Aabb box({14.0f, 2.0f, 0.0f}, {14.6f, 3.8f, 0.6f});
    MoveOptions opts;
    opts.skin = 1e-4f;

    SUBCASE("Stops against wall at x = 16 when crossing from chunk 0") {
        const MoveResult res = moveAabb(world, box, {5.0f, 0.0f, 0.0f}, opts);
        CHECK(res.blocked.x);
        CHECK(res.box.max.x == doctest::Approx(16.0f - opts.skin));
    }

    SUBCASE("Unloaded chunks are treated as air and traversed without errors") {
        // Move toward +Z (chunk {0, 0, 1} is not loaded)
        const MoveResult res = moveAabb(world, box, {0.0f, 0.0f, 30.0f}, opts);
        CHECK_FALSE(res.blocked.z);
        CHECK(res.moved.z == doctest::Approx(30.0f));
    }
}

TEST_CASE("moveAabb: concurrent delta clamping is thread-safe (atomic warnedClamp)") {
    auto isSolid = [](const glm::ivec3& b) { return b.y <= 0; };
    Aabb box({0.0f, 10.0f, 0.0f}, {1.0f, 11.0f, 1.0f});

    constexpr int kNumThreads = 8;
    std::vector<std::thread> threads;
    threads.reserve(kNumThreads);

    for (int t = 0; t < kNumThreads; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < 50; ++i) {
                const MoveResult res = moveAabb(box, {500.0f, 0.0f, 0.0f}, isSolid);
                CHECK(res.moved.x <= 256.0f);
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }
}

TEST_CASE("VoxelCollision: overlap detection and spawn backtrack against a wall") {
    TestBlockRegistry reg;
    VoxelWorld world(reg.registry);
    world.loadChunk({0, 0, 0});

    // Stone wall at x = 2 (block [2, 3])
    world.setBlock({2, 1, 0}, reg.stone);

    auto overlapsSolid = [&](const Aabb& aabb) {
        constexpr float kTiny = 1e-4f;
        const int minX = static_cast<int>(std::floor(aabb.min.x + kTiny));
        const int maxX = static_cast<int>(std::floor(aabb.max.x - kTiny));
        const int minY = static_cast<int>(std::floor(aabb.min.y + kTiny));
        const int maxY = static_cast<int>(std::floor(aabb.max.y - kTiny));
        const int minZ = static_cast<int>(std::floor(aabb.min.z + kTiny));
        const int maxZ = static_cast<int>(std::floor(aabb.max.z - kTiny));
        for (int y = minY; y <= maxY; ++y) {
            for (int z = minZ; z <= maxZ; ++z) {
                for (int x = minX; x <= maxX; ++x) {
                    if (world.isSolid(glm::ivec3(x, y, z))) {
                        return true;
                    }
                }
            }
        }
        return false;
    };

    const glm::vec3 halfExtents(0.25f);
    const glm::vec3 fwd(1.0f, 0.0f, 0.0f); // Facing the wall at x = 2

    // Player standing very close to the wall; spawn at (1.9, 1.5, 0.5)
    // Box would span x in [1.65, 2.15], penetrating the block at x = 2
    const glm::vec3 eyePos(1.9f, 1.5f, 0.5f);
    const Aabb initialBounds(eyePos - halfExtents, eyePos + halfExtents);
    CHECK(overlapsSolid(initialBounds));

    // Backtrack along -forward() up to 0.5 blocks
    glm::vec3 spawnPos = eyePos;
    constexpr float kMaxBacktrack = 0.5f;
    constexpr int kSteps = 10;
    bool foundFree = false;
    for (int i = 1; i <= kSteps; ++i) {
        const float backDist = (kMaxBacktrack / static_cast<float>(kSteps)) * static_cast<float>(i);
        const glm::vec3 candidatePos = eyePos - fwd * backDist;
        const Aabb candidateBounds(candidatePos - halfExtents, candidatePos + halfExtents);
        if (!overlapsSolid(candidateBounds)) {
            spawnPos = candidatePos;
            foundFree = true;
            break;
        }
    }

    CHECK(foundFree);
    CHECK(spawnPos.x <= 1.75f);
    CHECK_FALSE(overlapsSolid(Aabb(spawnPos - halfExtents, spawnPos + halfExtents)));
}
