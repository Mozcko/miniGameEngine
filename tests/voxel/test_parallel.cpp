// Generating and meshing on a JobSystem yields the exact same result as on the main thread (#56)
#include "VoxelTestWorld.h"

#include <doctest/doctest.h>
#include <mge/core/Jobs.h>
#include <mge/core/Random.h>
#include <mge/voxel/ChunkMesher.h>

#include <future>
#include <memory>
#include <vector>

using namespace mge;
using mge_test::TestBlocks;

namespace {
    // Reproducible random blocks: exercises borders, water, and glass
    class NoiseGenerator : public TerrainGenerator {
    public:
        explicit NoiseGenerator(const TestBlocks& b) : m_b(b) {}
        void generate(const ChunkCoord& coord, ChunkData& chunk) const override {
            const glm::ivec3 o = chunkOrigin(coord);
            for (int y = 0; y < kChunkSize; y++)
                for (int z = 0; z < kChunkSize; z++)
                    for (int x = 0; x < kChunkSize; x++) {
                        const std::uint32_t h = hash3i(o.x + x, o.y + y, o.z + z, 7) % 10;
                        chunk.set(x, y, z, h < 4 ? m_b.stone : h < 5 ? m_b.water : h < 6 ? m_b.glass : kAir);
                    }
        }

    private:
        const TestBlocks& m_b;
    };

    bool sameMesh(const MeshData& a, const MeshData& b) {
        return a.positions == b.positions && a.normals == b.normals && a.colors == b.colors &&
               a.uvs == b.uvs && a.indices == b.indices;
    }
}

TEST_CASE("Parallel: generate on a JobSystem and insert == loadChunk on the main thread") {
    TestBlocks b;
    VoxelWorld sequential(b.registry, std::make_unique<NoiseGenerator>(b));
    VoxelWorld parallel(b.registry, std::make_unique<NoiseGenerator>(b));
    std::vector<ChunkCoord> coords;
    for (int x = -2; x <= 2; x++)
        for (int y = -1; y <= 1; y++)
            for (int z = -2; z <= 2; z++) coords.push_back({x, y, z});

    JobSystem jobs(4);
    std::vector<std::future<ChunkData>> generated;
    const TerrainGenerator* generator = parallel.generator();
    for (const ChunkCoord& c : coords)
        generated.push_back(jobs.submit([generator, c] {
            ChunkData data;
            generator->generate(c, data);
            return data;
        }));
    for (std::size_t i = 0; i < coords.size(); i++) {
        parallel.insertChunk(coords[i], generated[i].get());
        sequential.loadChunk(coords[i]);
    }
    for (const ChunkCoord& c : coords) CHECK(parallel.chunk(c)->blocks() == sequential.chunk(c)->blocks());
    CHECK(parallel.dirtyChunks().size() == coords.size());
}

TEST_CASE("Parallel: meshing snapshots on a JobSystem == buildChunkMesh(world) (with AO and greedy)") {
    TestBlocks b;
    VoxelWorld world(b.registry, std::make_unique<NoiseGenerator>(b));
    for (int x = -1; x <= 1; x++)
        for (int y = -1; y <= 1; y++)
            for (int z = -1; z <= 1; z++) world.loadChunk({x, y, z});
    ChunkMeshOptions options;
    options.ambientOcclusion = true;

    JobSystem jobs(4);
    std::vector<ChunkCoord> coords = world.loadedChunks();
    std::vector<std::future<ChunkMesh>> meshes;
    for (const ChunkCoord& c : coords) {
        auto snapshot =
            std::make_shared<ChunkSnapshot>(ChunkSnapshot::capture(world, c)); // On the main thread
        meshes.push_back(
            jobs.submit([snapshot, &b, options] { return buildChunkMesh(*snapshot, b.registry, options); }));
    }
    for (std::size_t i = 0; i < coords.size(); i++) {
        const ChunkMesh fromJob = meshes[i].get();
        const ChunkMesh direct = buildChunkMesh(world, coords[i], options);
        CAPTURE(coords[i].x);
        CAPTURE(coords[i].y);
        CAPTURE(coords[i].z);
        CHECK(sameMesh(fromJob.opaque, direct.opaque));
        CHECK(sameMesh(fromJob.transparent, direct.transparent));
        CHECK(sameMesh(fromJob.cutout, direct.cutout));
    }
}

TEST_CASE("ChunkSnapshot: copies the chunk and neighbor borders; unloaded = air") {
    TestBlocks b;
    VoxelWorld world(b.registry);
    world.loadChunk({0, 0, 0});
    world.loadChunk({1, 0, 0});
    world.setBlock({3, 4, 5}, b.stone);
    world.setBlock({16, 7, 8}, b.glass); // Chunk to the right, first column
    world.setBlock({17, 7, 8}, b.water); // Outside the 1-block border
    const ChunkSnapshot snapshot = ChunkSnapshot::capture(world, {0, 0, 0});
    REQUIRE(snapshot.valid());
    CHECK(snapshot.get(3, 4, 5) == b.stone);
    CHECK(snapshot.get(16, 7, 8) == b.glass);
    CHECK(snapshot.get(-1, 0, 0) == kAir); // Unloaded chunk to the left
    CHECK_FALSE(ChunkSnapshot::capture(world, {9, 9, 9}).valid());
}
