#include "VoxelTestWorld.h"

#include <doctest/doctest.h>
#include <mge/core/math/Geometry.h>
#include <mge/voxel/ChunkLod.h>
#include <mge/voxel/ChunkSnapshot.h>
#include <mge/voxel/VoxelWorld.h>

using namespace mge;
using namespace mge_test;

TEST_CASE("voxel:ChunkLod: downsampling full, empty, and half-full chunks") {
    TestBlocks tb;
    VoxelWorld world(tb.registry);

    // 1. Empty chunk (all air)
    world.loadChunk({0, 0, 0});
    const ChunkSnapshot emptySnap = ChunkSnapshot::capture(world, {0, 0, 0});
    REQUIRE(emptySnap.valid());

    LodChunk emptyLod1 = downsample(emptySnap, tb.registry, 1);
    CHECK(emptyLod1.factor == 2);
    CHECK(emptyLod1.dimension == 8);
    for (int y = 0; y < 8; ++y) {
        for (int z = 0; z < 8; ++z) {
            for (int x = 0; x < 8; ++x) {
                CHECK(emptyLod1.get(x, y, z) == kAir);
            }
        }
    }

    LodChunk emptyLod2 = downsample(emptySnap, tb.registry, 2);
    CHECK(emptyLod2.factor == 4);
    CHECK(emptyLod2.dimension == 4);
    for (int y = 0; y < 4; ++y) {
        for (int z = 0; z < 4; ++z) {
            for (int x = 0; x < 4; ++x) {
                CHECK(emptyLod2.get(x, y, z) == kAir);
            }
        }
    }

    // 2. Full chunk (all stone)
    for (int y = 0; y < kChunkSize; ++y) {
        for (int z = 0; z < kChunkSize; ++z) {
            for (int x = 0; x < kChunkSize; ++x) {
                world.setBlock({x, y, z}, tb.stone);
            }
        }
    }
    const ChunkSnapshot fullSnap = ChunkSnapshot::capture(world, {0, 0, 0});
    LodChunk fullLod1 = downsample(fullSnap, tb.registry, 1);
    CHECK(fullLod1.dimension == 8);
    for (int y = 0; y < 8; ++y) {
        for (int z = 0; z < 8; ++z) {
            for (int x = 0; x < 8; ++x) {
                CHECK(fullLod1.get(x, y, z) == tb.stone);
            }
        }
    }

    LodChunk fullLod2 = downsample(fullSnap, tb.registry, 2);
    CHECK(fullLod2.dimension == 4);
    for (int y = 0; y < 4; ++y) {
        for (int z = 0; z < 4; ++z) {
            for (int x = 0; x < 4; ++x) {
                CHECK(fullLod2.get(x, y, z) == tb.stone);
            }
        }
    }

    // 3. Half-full chunk (flat ground up to y = 8)
    VoxelWorld flatWorld(tb.registry, std::make_unique<FlatGenerator>(tb.stone, 8));
    flatWorld.loadChunk({0, 0, 0});
    const ChunkSnapshot halfSnap = ChunkSnapshot::capture(flatWorld, {0, 0, 0});

    LodChunk halfLod1 = downsample(halfSnap, tb.registry, 1);
    CHECK(halfLod1.dimension == 8);
    for (int y = 0; y < 8; ++y) {
        for (int z = 0; z < 8; ++z) {
            for (int x = 0; x < 8; ++x) {
                if (y < 4) {
                    CHECK(halfLod1.get(x, y, z) == tb.stone);
                } else {
                    CHECK(halfLod1.get(x, y, z) == kAir);
                }
            }
        }
    }

    LodChunk halfLod2 = downsample(halfSnap, tb.registry, 2);
    CHECK(halfLod2.dimension == 4);
    for (int y = 0; y < 4; ++y) {
        for (int z = 0; z < 4; ++z) {
            for (int x = 0; x < 4; ++x) {
                if (y < 2) {
                    CHECK(halfLod2.get(x, y, z) == tb.stone);
                } else {
                    CHECK(halfLod2.get(x, y, z) == kAir);
                }
            }
        }
    }
}

TEST_CASE("voxel:ChunkLod: topmost opaque block rule") {
    TestBlocks tb;
    BlockDesc grassDesc;
    grassDesc.name = "grass";
    grassDesc.opaque = true;
    grassDesc.color = {0.2f, 0.8f, 0.2f, 1.0f};
    const BlockId grass = tb.registry.add(grassDesc);

    VoxelWorld world(tb.registry);
    world.loadChunk({0, 0, 0});

    // Cell 1 at (0, 0, 0) with factor 2 (x in [0, 1], y in [0, 1], z in [0, 1]):
    // Layer y = 1: water (translucent) at (0, 1, 0), air elsewhere.
    // Layer y = 0: stone (opaque) at (0, 0, 0).
    // The rule should discard water since it is not opaque and pick stone.
    world.setBlock({0, 1, 0}, tb.water);
    world.setBlock({0, 0, 0}, tb.stone);

    // Cell 2 at (2, 0, 0) (sx = 1, sy = 0, sz = 0):
    // Layer y = 1: flower (cutout / Cross shape) at (2, 1, 0).
    // Layer y = 0: stone at (2, 0, 0).
    // The rule should discard the flower and pick stone.
    world.setBlock({2, 1, 0}, tb.flower);
    world.setBlock({2, 0, 0}, tb.stone);

    // Cell 3 at (4, 0, 0) (sx = 2, sy = 0, sz = 0):
    // Layer y = 1: grass (opaque) at (4, 1, 0).
    // Layer y = 0: stone (opaque) at (4, 0, 0).
    // Should pick grass because it is above stone.
    world.setBlock({4, 1, 0}, grass);
    world.setBlock({4, 0, 0}, tb.stone);

    // Cell 4 at (6, 0, 0) (sx = 3, sy = 0, sz = 0):
    // Only glass and water (neither opaque).
    // Should return air.
    world.setBlock({6, 1, 0}, tb.glass);
    world.setBlock({6, 0, 0}, tb.water);

    const ChunkSnapshot snap = ChunkSnapshot::capture(world, {0, 0, 0});
    LodChunk lod = downsample(snap, tb.registry, 1);

    CHECK(lod.get(0, 0, 0) == tb.stone);
    CHECK(lod.get(1, 0, 0) == tb.stone);
    CHECK(lod.get(2, 0, 0) == grass);
    CHECK(lod.get(3, 0, 0) == kAir);
}

TEST_CASE("voxel:ChunkLod: LOD 1 triangle count <= 1/4 of LOD 0 (+ skirts)") {
    TestBlocks tb;
    VoxelWorld world(tb.registry, std::make_unique<FlatGenerator>(tb.stone, 8));
    world.loadChunk({0, 0, 0});
    const ChunkSnapshot snap = ChunkSnapshot::capture(world, {0, 0, 0});

    ChunkMeshOptions optNaive;
    optNaive.greedy = false;
    optNaive.lodSkirts = false;

    // Without greedy and without skirts: LOD 1 has 8x8 blocks instead of 16x16, so surface area
    // in each direction is reduced by exactly 4
    const ChunkMesh meshLod0 = buildLodChunkMesh(snap, tb.registry, 0, optNaive);
    const ChunkMesh meshLod1NoSkirts = buildLodChunkMesh(snap, tb.registry, 1, optNaive);

    const std::size_t tri0 = meshLod0.opaque.triangleCount();
    const std::size_t tri1NoSkirts = meshLod1NoSkirts.opaque.triangleCount();
    REQUIRE(tri0 > 0);
    CHECK(tri1NoSkirts <= tri0 / 4);

    // With skirts on all 4 edges: each 8-block edge adds 8 quads (16 triangles) = 64 triangles
    ChunkMeshOptions optWithSkirts = optNaive;
    optWithSkirts.lodSkirts = true;
    const ChunkMesh meshLod1WithSkirts = buildLodChunkMesh(snap, tb.registry, 1, optWithSkirts);
    const std::size_t tri1WithSkirts = meshLod1WithSkirts.opaque.triangleCount();
    CHECK(tri1WithSkirts <= tri0 / 4 + 64);
    CHECK(tri1WithSkirts > tri1NoSkirts);

    // With greedy meshing
    ChunkMeshOptions optGreedy;
    optGreedy.greedy = true;
    optGreedy.lodSkirts = true;
    const ChunkMesh meshGreedy = buildLodChunkMesh(snap, tb.registry, 1, optGreedy);
    CHECK(meshGreedy.opaque.triangleCount() <= tri1WithSkirts);
}

TEST_CASE("voxel:ChunkLod: mesh validation (validate passes without errors)") {
    TestBlocks tb;
    VoxelWorld world(tb.registry, std::make_unique<FlatGenerator>(tb.stone, 8));
    world.loadChunk({0, 0, 0});

    // Add a few varied blocks
    world.setBlock({3, 9, 3}, tb.stone);
    world.setBlock({4, 9, 3}, tb.stone);
    world.setBlock({5, 8, 5}, tb.water); // water generates a transparent top face at LOD > 0

    const ChunkSnapshot snap = ChunkSnapshot::capture(world, {0, 0, 0});

    // LOD 0
    const ChunkMesh mesh0 = buildLodChunkMesh(snap, tb.registry, 0);
    CHECK(mesh0.opaque.validate().empty());

    // LOD 1
    const ChunkMesh mesh1 = buildLodChunkMesh(snap, tb.registry, 1);
    CHECK(mesh1.opaque.validate().empty());
    CHECK(mesh1.cutout.empty());
    CHECK(mesh1.transparent.validate().empty());

    // LOD 2
    const ChunkMesh mesh2 = buildLodChunkMesh(snap, tb.registry, 2);
    CHECK(mesh2.opaque.validate().empty());
    CHECK(mesh2.cutout.empty());
    CHECK(mesh2.transparent.validate().empty());
}

TEST_CASE("voxel:ChunkLod: seams: two neighboring chunks at the same LOD have no internal faces") {
    TestBlocks tb;
    VoxelWorld world(tb.registry, std::make_unique<FlatGenerator>(tb.stone, 8));
    world.loadChunk({0, 0, 0});
    world.loadChunk({1, 0, 0}); // Neighbor in +X

    const ChunkSnapshot snapA = ChunkSnapshot::capture(world, {0, 0, 0});
    const ChunkSnapshot snapB = ChunkSnapshot::capture(world, {1, 0, 0});

    ChunkMeshOptions options;
    options.greedy = true;
    options.lodSkirts = true;

    const ChunkMesh meshA = buildLodChunkMesh(snapA, tb.registry, 1, options);
    const ChunkMesh meshB = buildLodChunkMesh(snapB, tb.registry, 1, options);

    // In chunk A, the boundary with B is x = 16. There must be no vertices or +X-facing faces at x = 16
    for (std::size_t i = 0; i < meshA.opaque.positions.size(); ++i) {
        const glm::vec3& pos = meshA.opaque.positions[i];
        const glm::vec3& norm = meshA.opaque.normals[i];
        if (norm.x > 0.9f) {
            // No +X-facing face on the shared boundary
            CHECK(pos.x < 15.99f);
        }
    }

    // In chunk B, the boundary with A is x = 0. There must be no -X-facing faces at x = 0
    for (std::size_t i = 0; i < meshB.opaque.positions.size(); ++i) {
        const glm::vec3& pos = meshB.opaque.positions[i];
        const glm::vec3& norm = meshB.opaque.normals[i];
        if (norm.x < -0.9f) {
            // No -X-facing face on the shared boundary
            CHECK(pos.x > 0.01f);
        }
    }
}

TEST_CASE("voxel:ChunkLod: skirts present on exposed edges") {
    TestBlocks tb;
    VoxelWorld world(tb.registry, std::make_unique<FlatGenerator>(tb.stone, 8));
    world.loadChunk({0, 0, 0}); // Isolated chunk: all 4 side borders face air

    const ChunkSnapshot snap = ChunkSnapshot::capture(world, {0, 0, 0});

    // With skirts (factor 2): there should be vertices extending down to y = -2
    ChunkMeshOptions optWithSkirts;
    optWithSkirts.lodSkirts = true;
    const ChunkMesh meshWithSkirts = buildLodChunkMesh(snap, tb.registry, 1, optWithSkirts);

    bool hasSkirtVertices = false;
    for (const auto& pos : meshWithSkirts.opaque.positions) {
        if (pos.y < -0.01f) {
            hasSkirtVertices = true;
            CHECK(pos.y >= -2.01f);
        }
    }
    CHECK(hasSkirtVertices);

    // Without skirts: there should be no vertex with y < 0
    ChunkMeshOptions optNoSkirts;
    optNoSkirts.lodSkirts = false;
    const ChunkMesh meshNoSkirts = buildLodChunkMesh(snap, tb.registry, 1, optNoSkirts);

    for (const auto& pos : meshNoSkirts.opaque.positions) {
        CHECK(pos.y >= -0.01f);
    }
}

TEST_CASE("voxel:ChunkLod: water at LOD > 0 generates transparent surface mesh") {
    TestBlocks tb;
    VoxelWorld world(tb.registry);
    world.loadChunk({0, 0, 0});

    // Stone up to y = 4, water at y = 5..7
    for (int y = 0; y <= 4; ++y) {
        for (int z = 0; z < kChunkSize; ++z) {
            for (int x = 0; x < kChunkSize; ++x) {
                world.setBlock({x, y, z}, tb.stone);
            }
        }
    }
    for (int y = 5; y <= 7; ++y) {
        for (int z = 0; z < kChunkSize; ++z) {
            for (int x = 0; x < kChunkSize; ++x) {
                world.setBlock({x, y, z}, tb.water);
            }
        }
    }

    const ChunkSnapshot snap = ChunkSnapshot::capture(world, {0, 0, 0});
    REQUIRE(snap.valid());

    const ChunkMesh meshLod1 = buildLodChunkMesh(snap, tb.registry, 1);
    CHECK_FALSE(meshLod1.transparent.empty());
    CHECK(meshLod1.transparent.vertexCount() > 0);

    for (const auto& pos : meshLod1.transparent.positions) {
        CHECK(pos.y == doctest::Approx(8.0f));
    }
}

TEST_CASE("voxel:ChunkLod: flat surface emits top faces at exact y_top (y=5 -> 6, y=4 -> 5)") {
    TestBlocks tb;

    for (const int topBlockY : {5, 4}) {
        const float expectedTopY = static_cast<float>(topBlockY + 1);
        VoxelWorld world(tb.registry, std::make_unique<FlatGenerator>(tb.stone, topBlockY + 1));
        world.loadChunk({0, 0, 0});
        const ChunkSnapshot snap = ChunkSnapshot::capture(world, {0, 0, 0});
        REQUIRE(snap.valid());

        for (const int lod : {1, 2}) {
            for (const bool greedy : {false, true}) {
                ChunkMeshOptions opt;
                opt.greedy = greedy;
                opt.lodSkirts = false;
                const ChunkMesh mesh = buildLodChunkMesh(snap, tb.registry, lod, opt);
                CHECK(mesh.opaque.validate().empty());

                int topVertices = 0;
                for (std::size_t i = 0; i < mesh.opaque.positions.size(); ++i) {
                    if (mesh.opaque.normals[i].y > 0.9f) {
                        CHECK(mesh.opaque.positions[i].y == doctest::Approx(expectedTopY));
                        topVertices++;
                    }
                }
                CHECK(topVertices > 0);
                // With greedy on a uniform flat surface, the top face merges into 1 quad (4 vertices)
                if (greedy) {
                    CHECK(topVertices == 4);
                }
            }
        }
    }
}

TEST_CASE("voxel:ChunkLod: two-level terrain (y=4 and y=9) has no cracks in LOD 1 and LOD 2") {
    TestBlocks tb;
    VoxelWorld world(tb.registry);
    world.loadChunk({0, 0, 0});

    // Left half (x < 8): blocks 0..4 (top at y = 5); right half (x >= 8): blocks 0..9 (top at y = 10)
    for (int z = 0; z < kChunkSize; ++z) {
        for (int x = 0; x < kChunkSize; ++x) {
            const int maxY = (x < 8) ? 4 : 9;
            for (int y = 0; y <= maxY; ++y) {
                world.setBlock({x, y, z}, tb.stone);
            }
        }
    }
    const ChunkSnapshot snap = ChunkSnapshot::capture(world, {0, 0, 0});
    REQUIRE(snap.valid());

    for (const int lod : {1, 2}) {
        ChunkMeshOptions opt;
        opt.greedy = true;
        opt.lodSkirts = false;
        const ChunkMesh mesh = buildLodChunkMesh(snap, tb.registry, lod, opt);
        CHECK(mesh.opaque.validate().empty());

        // 1. Downward vertical raycast at 256 points (16x16): always hits a top face at the exact height
        for (int z = 0; z < kChunkSize; ++z) {
            for (int x = 0; x < kChunkSize; ++x) {
                const float expectedY = (x < 8) ? 5.0f : 10.0f;
                const Ray ray{glm::vec3(static_cast<float>(x) + 0.5f, 20.0f, static_cast<float>(z) + 0.5f),
                              glm::vec3(0.0f, -1.0f, 0.0f)};
                float bestHitY = -1.0f;
                for (std::size_t t = 0; t + 2 < mesh.opaque.indices.size(); t += 3) {
                    const std::uint32_t i0 = mesh.opaque.indices[t];
                    const std::uint32_t i1 = mesh.opaque.indices[t + 1];
                    const std::uint32_t i2 = mesh.opaque.indices[t + 2];
                    if (mesh.opaque.normals[i0].y <= 0.9f) continue;
                    const auto hit = intersectRayTriangle(
                        ray, mesh.opaque.positions[i0], mesh.opaque.positions[i1], mesh.opaque.positions[i2]);
                    if (hit) {
                        const float hitY = ray.at(*hit).y;
                        if (hitY > bestHitY) bestHitY = hitY;
                    }
                }
                CHECK(bestHitY == doctest::Approx(expectedY));
            }
        }

        // 2. Horizontal raycast against the step at x = 8 between y = 5 and y = 10: no cracks
        for (int yStep = 5; yStep < 10; ++yStep) {
            for (int z = 0; z < kChunkSize; z += 2) {
                const Ray ray{glm::vec3(7.5f, static_cast<float>(yStep) + 0.5f, static_cast<float>(z) + 0.5f),
                              glm::vec3(1.0f, 0.0f, 0.0f)};
                bool hitStep = false;
                for (std::size_t t = 0; t + 2 < mesh.opaque.indices.size(); t += 3) {
                    const std::uint32_t i0 = mesh.opaque.indices[t];
                    const std::uint32_t i1 = mesh.opaque.indices[t + 1];
                    const std::uint32_t i2 = mesh.opaque.indices[t + 2];
                    if (mesh.opaque.normals[i0].x >= -0.9f) continue;
                    const auto hit = intersectRayTriangle(
                        ray, mesh.opaque.positions[i0], mesh.opaque.positions[i1], mesh.opaque.positions[i2]);
                    if (hit && std::abs(ray.at(*hit).x - 8.0f) < 1e-3f) {
                        hitStep = true;
                        break;
                    }
                }
                CHECK(hitStep);
            }
        }
    }
}

TEST_CASE(
    "voxel:ChunkLod: shallow water (stone y<=6, water y=7) stays above the opaque face in LOD 1 and 2") {
    TestBlocks tb;
    VoxelWorld world(tb.registry);
    world.loadChunk({0, 0, 0});

    for (int z = 0; z < kChunkSize; ++z) {
        for (int x = 0; x < kChunkSize; ++x) {
            for (int y = 0; y <= 6; ++y) {
                world.setBlock({x, y, z}, tb.stone);
            }
            world.setBlock({x, 7, z}, tb.water);
        }
    }
    const ChunkSnapshot snap = ChunkSnapshot::capture(world, {0, 0, 0});
    REQUIRE(snap.valid());

    for (const int lod : {1, 2}) {
        const ChunkMesh mesh = buildLodChunkMesh(snap, tb.registry, lod);
        CHECK(mesh.opaque.validate().empty());
        CHECK(mesh.transparent.validate().empty());
        REQUIRE_FALSE(mesh.transparent.empty());

        float minWaterY = 1e9f;
        for (const auto& pos : mesh.transparent.positions) {
            minWaterY = std::min(minWaterY, pos.y);
            CHECK(pos.y == doctest::Approx(8.0f));
        }
        CHECK(minWaterY == doctest::Approx(8.0f));

        float maxOpaqueTopY = -1e9f;
        for (std::size_t i = 0; i < mesh.opaque.positions.size(); ++i) {
            if (mesh.opaque.normals[i].y > 0.9f) {
                maxOpaqueTopY = std::max(maxOpaqueTopY, mesh.opaque.positions[i].y);
            }
        }
        CHECK(maxOpaqueTopY == doctest::Approx(7.0f));
        CHECK(maxOpaqueTopY < minWaterY);
    }
}

TEST_CASE("voxel:ChunkLod: shore half land y=7 and half water y=7 over stone y<=5 in LOD 1 and 2") {
    TestBlocks tb;
    VoxelWorld world(tb.registry);
    world.loadChunk({0, 0, 0});

    // Left half (x < 8): opaque land 0..7 (top at 8)
    // Right half (x >= 8): stone 0..5 (opaque top at 6) + water 6..7 (transparent quad at 8)
    for (int z = 0; z < kChunkSize; ++z) {
        for (int x = 0; x < kChunkSize; ++x) {
            if (x < 8) {
                for (int y = 0; y <= 7; ++y) {
                    world.setBlock({x, y, z}, tb.stone);
                }
            } else {
                for (int y = 0; y <= 5; ++y) {
                    world.setBlock({x, y, z}, tb.stone);
                }
                world.setBlock({x, 6, z}, tb.water);
                world.setBlock({x, 7, z}, tb.water);
            }
        }
    }
    const ChunkSnapshot snap = ChunkSnapshot::capture(world, {0, 0, 0});
    REQUIRE(snap.valid());

    for (const int lod : {1, 2}) {
        ChunkMeshOptions opt;
        opt.greedy = true;
        opt.lodSkirts = false;
        const ChunkMesh mesh = buildLodChunkMesh(snap, tb.registry, lod, opt);
        CHECK(mesh.opaque.validate().empty());
        CHECK(mesh.transparent.validate().empty());
        REQUIRE_FALSE(mesh.transparent.empty());

        // Water quad only on the right half at y = 8
        for (const auto& pos : mesh.transparent.positions) {
            CHECK(pos.x >= 7.99f);
            CHECK(pos.y == doctest::Approx(8.0f));
        }

        // Downward raycast on the opaque mesh: x < 8 hits at y = 8; x >= 8 hits at y = 6
        for (int z = 0; z < kChunkSize; z += 2) {
            for (int x = 0; x < kChunkSize; x += 2) {
                const float expectedOpaqueY = (x < 8) ? 8.0f : 6.0f;
                const Ray ray{glm::vec3(static_cast<float>(x) + 0.5f, 20.0f, static_cast<float>(z) + 0.5f),
                              glm::vec3(0.0f, -1.0f, 0.0f)};
                float bestHitY = -1.0f;
                for (std::size_t t = 0; t + 2 < mesh.opaque.indices.size(); t += 3) {
                    const std::uint32_t i0 = mesh.opaque.indices[t];
                    const std::uint32_t i1 = mesh.opaque.indices[t + 1];
                    const std::uint32_t i2 = mesh.opaque.indices[t + 2];
                    if (mesh.opaque.normals[i0].y <= 0.9f) continue;
                    const auto hit = intersectRayTriangle(
                        ray, mesh.opaque.positions[i0], mesh.opaque.positions[i1], mesh.opaque.positions[i2]);
                    if (hit) {
                        bestHitY = std::max(bestHitY, ray.at(*hit).y);
                    }
                }
                CHECK(bestHitY == doctest::Approx(expectedOpaqueY));
            }
        }
    }
}

TEST_CASE("voxel:ChunkLod: terrain above an underground lake keeps its top face in LOD 1 and 2") {
    TestBlocks tb;
    VoxelWorld world(tb.registry);
    world.loadChunk({0, 0, 0});

    for (int z = 0; z < kChunkSize; ++z) {
        for (int x = 0; x < kChunkSize; ++x) {
            for (int y = 0; y <= 2; ++y) {
                world.setBlock({x, y, z}, tb.stone);
            }
            world.setBlock({x, 3, z}, tb.water);
            // y 4..11: air (default)
            world.setBlock({x, 12, z}, tb.stone);
            world.setBlock({x, 13, z}, tb.stone);
        }
    }
    const ChunkSnapshot snap = ChunkSnapshot::capture(world, {0, 0, 0});
    REQUIRE(snap.valid());

    for (const int lod : {1, 2}) {
        ChunkMeshOptions opt;
        opt.greedy = true;
        opt.lodSkirts = false;
        const ChunkMesh mesh = buildLodChunkMesh(snap, tb.registry, lod, opt);
        CHECK(mesh.opaque.validate().empty());
        CHECK(mesh.transparent.validate().empty());

        int topVerticesAt14 = 0;
        for (std::size_t i = 0; i < mesh.opaque.positions.size(); ++i) {
            if (mesh.opaque.normals[i].y > 0.9f && mesh.opaque.positions[i].y == doctest::Approx(14.0f)) {
                ++topVerticesAt14;
            }
        }
        CHECK(topVerticesAt14 > 0);
    }
}
