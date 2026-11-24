#include <doctest/doctest.h>
#include <game/SandboxBlocks.h>
#include <game/SandboxTerrain.h>
#include <mge/voxel/ChunkLod.h>
#include <mge/voxel/ChunkSnapshot.h>

#include <array>
#include <cstdlib>
#include <initializer_list>
#include <memory>
#include <utility>

using namespace mge;
using sandbox::Biome;
using sandbox::SandboxTerrain;
using sandbox::TerrainSettings;

// Generation only (CPU): no window or OpenGL context.

namespace {
    TerrainSettings withSeed(std::uint32_t seed, bool caves = true) {
        TerrainSettings settings;
        settings.seed = seed;
        settings.caves = caves;
        return settings;
    }

    struct Fixture {
        BlockRegistry registry;
        sandbox::SandboxBlocks b = sandbox::registerSandboxBlocks(registry);

        ChunkData generate(const ChunkCoord& coord, TerrainSettings settings = {}) const {
            ChunkData chunk;
            SandboxTerrain(b, settings).generate(coord, chunk);
            return chunk;
        }

        // World of columns cx, cz in [-r, r] with all vertical chunks, loaded in the specified order
        std::unique_ptr<VoxelWorld> world(int r, bool reverse = false, TerrainSettings settings = {}) const {
            auto w = std::make_unique<VoxelWorld>(registry, std::make_unique<SandboxTerrain>(b, settings));
            for (int i = -r; i <= r; i++)
                for (int k = -r; k <= r; k++)
                    for (int cy = 0; cy < SandboxTerrain::kWorldChunksY; cy++)
                        w->loadChunk(reverse ? ChunkCoord{-i, SandboxTerrain::kWorldChunksY - 1 - cy, -k}
                                             : ChunkCoord{i, cy, k});
            return w;
        }
    };
}

TEST_CASE("Terrain: deterministic, distinct across chunks and by seed") {
    Fixture f;
    for (const ChunkCoord& c : {ChunkCoord{0, 1, 0}, ChunkCoord{3, 0, -2}, ChunkCoord{-7, 2, 11}}) {
        CAPTURE(c.x);
        CAPTURE(c.y);
        CAPTURE(c.z);
        CHECK(f.generate(c).blocks() == f.generate(c).blocks());
    }
    CHECK(f.generate({0, 1, 0}).blocks() != f.generate({1, 1, 0}).blocks());
    CHECK(f.generate({0, 1, 0}, withSeed(1)).blocks() != f.generate({0, 1, 0}, withSeed(2)).blocks());
    // Height now depends on the seed (noise)
    const SandboxTerrain a(f.b, withSeed(1)), b(f.b, withSeed(2));
    int differ = 0;
    for (int x = 0; x < 64; x += 4) differ += a.surfaceHeight(x, 0) != b.surfaceHeight(x, 0);
    CHECK(differ > 8);
}

TEST_CASE("Terrain: below the world is stone; above the world is empty") {
    Fixture f;
    const ChunkData below = f.generate({0, -1, 0});
    for (BlockId id : below.blocks()) CHECK(id == f.b.stone);
    for (int cx = -3; cx < 3; cx++) {
        const ChunkData above = f.generate({cx, SandboxTerrain::kWorldChunksY, 0});
        CHECK(above.isEmpty());
    }
}

TEST_CASE("Terrain: heights within range and without repeating like previous waves") {
    Fixture f;
    const SandboxTerrain terrain(f.b);
    int minH = 1000, maxH = -1;
    for (int x = -2000; x < 2000; x += 13)
        for (int z = -2000; z < 2000; z += 17) {
            const int h = terrain.surfaceHeight(x, z);
            CHECK(h >= 1);
            CHECK(h <= SandboxTerrain::kMaxSurfaceHeight);
            minH = std::min(minH, h);
            maxH = std::max(maxH, h);
        }
    CAPTURE(minH);
    CAPTURE(maxH);
    CHECK(minH < SandboxTerrain::kWaterLevel);      // Has sea
    CHECK(maxH > SandboxTerrain::kWaterLevel + 20); // Has mountains
}

TEST_CASE("Terrain: all biomes appear across a large area") {
    Fixture f;
    const SandboxTerrain terrain(f.b);
    std::array<int, 4> counts{};
    int total = 0;
    for (int x = -3000; x < 3000; x += 24)
        for (int z = -3000; z < 3000; z += 24) {
            counts[static_cast<int>(terrain.biome(x, z))]++;
            total++;
        }
    const char* names[] = {"Plains", "Desert", "Mountains", "Shore"};
    for (int i = 0; i < 4; i++) {
        CAPTURE(names[i]);
        CAPTURE(counts[i]);
        CHECK(counts[i] > total / 50); // Each > 2%
    }
    CHECK(counts[static_cast<int>(Biome::Plains)] > total / 5); // Plains dominate
}

TEST_CASE("Terrain: block rules in a 5x5 column world") {
    Fixture f;
    const SandboxTerrain terrain(f.b);
    const auto world = f.world(2);
    for (int x = -32; x < 48; x++) {
        for (int z = -32; z < 48; z++) {
            CHECK(world->getBlock(x, 0, z) == f.b.stone); // Caves never break the floor
            const int h = terrain.surfaceHeight(x, z);
            for (int y = 1; y < SandboxTerrain::kWorldHeight; y++) {
                const BlockId id = world->getBlock(x, y, z);
                if (id == f.b.water) {
                    CHECK(y <= SandboxTerrain::kWaterLevel); // Water never exceeds sea level
                    CHECK(y > h);                            // and is only above the surface
                }
                if (y == h) CHECK(id != kAir); // Surface never sinks into a cave
            }
        }
    }
}

TEST_CASE("Terrain: caves exist underground, and none when disabled") {
    Fixture f;
    const SandboxTerrain terrain(f.b);
    long underground = 0, carved = 0;
    for (int x = 0; x < 96; x++)
        for (int z = 0; z < 96; z++) {
            const int h = terrain.surfaceHeight(x, z);
            for (int y = 1; y <= h - 4; y++) {
                underground++;
                carved += terrain.isCave(x, y, z, h);
            }
        }
    CAPTURE(carved);
    CAPTURE(underground);
    CHECK(carved > underground / 200); // > 0.5%
    CHECK(carved < underground / 5);   // But underground remains mostly stone

    const SandboxTerrain noCaves(f.b, withSeed(SandboxTerrain::kDefaultSeed, false));
    for (int x = 0; x < 32; x++)
        for (int y = 1; y < 30; y++) CHECK_FALSE(noCaves.isCave(x, y, 0, 50));
}

TEST_CASE("Terrain: decoration generates trees, roses, and flowers on 1-3% of grass (regression #5)") {
    Fixture f;
    const SandboxTerrain terrain(f.b);
    const auto world = f.world(5);
    long logs = 0, leaves = 0, roses = 0, flowers = 0, trees = 0, grassColumns = 0;
    for (int x = -80; x < 96; x++) {
        for (int z = -80; z < 96; z++) {
            const int h = terrain.surfaceHeight(x, z);
            if (world->getBlock(x, h, z) != f.b.grass) continue;
            grassColumns++;
            const BlockId above = world->getBlock(x, h + 1, z);
            trees += above == f.b.log; // Log on grass = tree
            roses += above == f.b.rose;
            flowers += above == f.b.flower;
            for (int y = h + 1; y < h + 9; y++) {
                logs += world->getBlock(x, y, z) == f.b.log;
                leaves += world->getBlock(x, y, z) == f.b.leaves;
            }
        }
    }
    CAPTURE(grassColumns);
    CAPTURE(trees);
    CAPTURE(roses);
    CAPTURE(flowers);
    CHECK(logs > 0);
    CHECK(leaves > 0);
    REQUIRE(grassColumns > 5000);
    for (long count : {trees, roses, flowers}) {
        const double rate = double(count) / double(grassColumns);
        CHECK(rate > 0.01);
        CHECK(rate < 0.03);
    }
}

TEST_CASE("Terrain: distant coordinates do not break generation (no overflow)") {
    Fixture f;
    const ChunkData far = f.generate({100000, 0, -100000});
    CHECK(f.registry.isSolid(far.get(0, 0, 0)));
}

TEST_CASE("Terrain: trees cross chunk borders intact (KI#6)") {
    Fixture f;
    const SandboxTerrain terrain(f.b);
    const auto world = f.world(3);
    int trees = 0, borderTrees = 0;
    // For each trunk on grass away from the loaded area edge: its entire canopy is in the world
    for (int x = -46; x < 62; x++) {
        for (int z = -46; z < 62; z++) {
            const int h = terrain.surfaceHeight(x, z);
            if (world->getBlock(x, h, z) != f.b.grass || world->getBlock(x, h + 1, z) != f.b.log) continue;
            trees++;
            int trunk = 0;
            while (world->getBlock(x, h + 1 + trunk, z) == f.b.log) trunk++;
            const int top = h + trunk; // Top block of the trunk
            const int lx = floorMod(x, kChunkSize), lz = floorMod(z, kChunkSize),
                      ly = floorMod(top, kChunkSize);
            if (lx < 2 || lx > 13 || lz < 2 || lz > 13 || ly > 13) borderTrees++;
            // Top canopy layer (above the trunk): full diamond of leaves
            for (int dx = -2; dx <= 2; dx++)
                for (int dz = -2; dz <= 2; dz++) {
                    if (std::abs(dx) + std::abs(dz) >= 4) continue;
                    const BlockId id = world->getBlock(x + dx, top + 2, z + dz);
                    CAPTURE(x);
                    CAPTURE(z);
                    CHECK((id == f.b.leaves || id == f.b.log));
                }
        }
    }
    CAPTURE(trees);
    CHECK(trees > 20);
    CHECK(borderTrees > 5); // There are trees touching another chunk (sideways or above)
}

TEST_CASE("Terrain: world is identical regardless of load order") {
    Fixture f;
    const auto a = f.world(2, false);
    const auto b = f.world(2, true);
    for (const ChunkCoord& c : a->loadedChunks()) CHECK(a->chunk(c)->blocks() == b->chunk(c)->blocks());
}

TEST_CASE("Terrain: LOD 1 and LOD 2 triangle counts (seed 1337, 9 columns)") {
    Fixture f;
    // Load radius 2 so the 9 central columns (-1..1 x -1..1) have complete neighbors
    const auto world = f.world(2, false, withSeed(SandboxTerrain::kDefaultSeed));

    ChunkMeshOptions options;
    options.atlas = {sandbox::kAtlasColumns, sandbox::kAtlasRows};
    options.ambientOcclusion = true;
    options.lodSkirts = true;

    std::size_t trisLod1 = 0;
    std::size_t trisLod2 = 0;
    for (int cx = -1; cx <= 1; ++cx) {
        for (int cz = -1; cz <= 1; ++cz) {
            for (int cy = 0; cy < SandboxTerrain::kWorldChunksY; ++cy) {
                const ChunkSnapshot snap = ChunkSnapshot::capture(*world, {cx, cy, cz});
                REQUIRE(snap.valid());
                const ChunkMesh m1 = buildLodChunkMesh(snap, f.registry, 1, options);
                const ChunkMesh m2 = buildLodChunkMesh(snap, f.registry, 2, options);
                CHECK(m1.opaque.validate().empty());
                CHECK(m2.opaque.validate().empty());
                trisLod1 += m1.opaque.triangleCount();
                trisLod2 += m2.opaque.triangleCount();
            }
        }
    }

    MESSAGE("SandboxTerrain seed 1337 (9 columns): LOD 1 = ", trisLod1, " tris, LOD 2 = ", trisLod2, " tris");
    // Baseline on main (seed 1337, 9 columns): LOD 1 = 2934 tris, LOD 2 = 730 tris.
    // LOD 1 stays within +10% (<= 3227); at LOD 2 (f = 4), 1..3 block height steps within
    // each band add ~90 required side quads to prevent cracks (<= 950).
    CHECK(trisLod1 > 0);
    CHECK(trisLod1 <= 3227);
    CHECK(trisLod2 > 0);
    CHECK(trisLod2 <= 950);
}
