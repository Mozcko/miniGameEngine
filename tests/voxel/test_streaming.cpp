// ChunkStreamer (#57): rings with hysteresis, memory budget, pool, stale results, with and without threads
#include "VoxelTestWorld.h"

#include <doctest/doctest.h>
#include <mge/core/Jobs.h>
#include <mge/voxel/ChunkStreamer.h>

#include <atomic>
#include <chrono>
#include <map>
#include <memory>
#include <thread>

using namespace mge;
using mge_test::TestBlocks;

namespace {
    // Flat 20-block-high ground that counts how many times each chunk was generated
    class CountingGenerator : public TerrainGenerator {
    public:
        CountingGenerator(BlockId block, std::atomic<int>& count) : m_flat(block, 20), m_count(count) {}
        void generate(const ChunkCoord& coord, ChunkData& chunk) const override {
            m_count++;
            m_flat.generate(coord, chunk);
        }

    private:
        mge_test::FlatGenerator m_flat;
        std::atomic<int>& m_count;
    };

    StreamingSettings smallSettings() {
        StreamingSettings s;
        s.loadRadius = 3;
        s.meshRadius = 2;
        s.unloadMargin = 2;
        s.minChunkY = 0;
        s.maxChunkY = 1;
        s.maxResultsPerUpdate = 64;
        return s;
    }

    // Calls update until nothing remains to do (with threads, waiting for jobs to finish);
    // accumulates delivered meshes per chunk and optionally their LOD level
    void converge(ChunkStreamer& streamer, const ChunkCoord& center,
                  std::map<ChunkCoord, ChunkMesh>* meshes = nullptr,
                  std::map<ChunkCoord, int>* lods = nullptr) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
        while (std::chrono::steady_clock::now() < deadline) {
            StreamUpdate u = streamer.update(center);
            if (meshes || lods) {
                for (auto& [c, m, lod] : u.meshes) {
                    if (meshes) (*meshes)[c] = std::move(m);
                    if (lods) (*lods)[c] = lod;
                }
                for (const ChunkCoord& c : u.unloaded) {
                    if (meshes) meshes->erase(c);
                    if (lods) lods->erase(c);
                }
            }
            if (streamer.idle()) return;
            const StreamingStats stats = streamer.stats();
            if (stats.pendingGenerate + stats.pendingMesh > 0)
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        FAIL("Streamer did not finish within 30 s");
    }
}

TEST_CASE("Streamer: loads loadRadius, meshes meshRadius, and becomes idle") {
    TestBlocks b;
    std::atomic<int> generated{0};
    VoxelWorld world(b.registry, std::make_unique<CountingGenerator>(b.stone, generated));
    ChunkStreamer streamer(world, smallSettings());
    std::map<ChunkCoord, ChunkMesh> meshes;
    converge(streamer, {0, 0, 0}, &meshes);
    CHECK(world.chunkCount() == 7 * 7 * 2); // Radius 3, height 2
    CHECK(meshes.size() == 5 * 5 * 2);      // Mesh radius 2
    CHECK(generated.load() == 7 * 7 * 2);
    CHECK(world.dirtyChunks().size() == 7 * 7 * 2 - 5 * 5 * 2); // Only outer chunks remain unmeshed
}

TEST_CASE("Streamer: memory stays bounded while traveling 2000 blocks; pool is reused") {
    TestBlocks b;
    std::atomic<int> generated{0};
    VoxelWorld world(b.registry, std::make_unique<CountingGenerator>(b.stone, generated));
    const StreamingSettings settings = smallSettings();
    ChunkStreamer streamer(world, settings);
    const int unloadSide = 2 * (settings.loadRadius + settings.unloadMargin) + 1;
    const std::size_t bound = std::size_t(unloadSide) * unloadSide * 2;
    std::size_t peak = 0;
    for (int x = 0; x <= 125; x++) { // 125 chunks = 2000 blocks
        for (int i = 0; i < 3; i++) streamer.update({x, 0, 0});
        peak = std::max(peak, world.chunkCount());
    }
    CAPTURE(peak);
    CHECK(peak <= bound);
    CHECK(world.reusedCount() > 0); // Loads reused unloaded chunks
    CHECK(world.isLoaded({125, 0, 0}));
    CHECK_FALSE(world.isLoaded({0, 0, 0}));
}

TEST_CASE("Streamer: hysteresis, oscillating across a border neither unloads nor regenerates") {
    TestBlocks b;
    std::atomic<int> generated{0};
    VoxelWorld world(b.registry, std::make_unique<CountingGenerator>(b.stone, generated));
    ChunkStreamer streamer(world, smallSettings());
    converge(streamer, {0, 0, 0});
    converge(streamer, {1, 0, 0});
    const int afterFirstMove = generated.load();
    std::size_t unloads = 0;
    for (int i = 0; i < 50; i++) {
        unloads += streamer.update({i % 2, 0, 0}).unloaded.size();
    }
    CHECK(unloads == 0);
    CHECK(generated.load() == afterFirstMove);
}

TEST_CASE("Streamer: memory budget (maxLoadedChunks)") {
    TestBlocks b;
    std::atomic<int> generated{0};
    VoxelWorld world(b.registry, std::make_unique<CountingGenerator>(b.stone, generated));
    StreamingSettings settings = smallSettings();
    settings.maxLoadedChunks = 60; // Less than 7x7x2 = 98
    ChunkStreamer streamer(world, settings);
    for (int i = 0; i < 200; i++) {
        streamer.update({0, 0, 0});
        CHECK(world.chunkCount() <= 60);
    }
    // Nearby chunks (mesh radius 2: 5x5x2 = 50) fit completely
    for (int x = -2; x <= 2; x++)
        for (int z = -2; z <= 2; z++) CHECK(world.isLoaded({x, 0, z}));
}

TEST_CASE("Streamer: with JobSystem produces the same world and meshes as single-threaded") {
    TestBlocks b;
    std::atomic<int> countA{0}, countB{0};
    VoxelWorld a(b.registry, std::make_unique<CountingGenerator>(b.stone, countA));
    VoxelWorld c(b.registry, std::make_unique<CountingGenerator>(b.stone, countB));
    JobSystem jobs(3);
    StreamingSettings settings = smallSettings();
    settings.meshOptions.ambientOcclusion = true;
    ChunkStreamer sync(a, settings);
    ChunkStreamer async(c, settings, &jobs);
    std::map<ChunkCoord, ChunkMesh> meshesA, meshesB;
    converge(sync, {2, 0, -1}, &meshesA);
    converge(async, {2, 0, -1}, &meshesB);
    REQUIRE(meshesA.size() == meshesB.size());
    for (const auto& entry : meshesA) {
        const ChunkCoord& coord = entry.first;
        const ChunkMesh& mesh = entry.second;
        CAPTURE(coord.x);
        CAPTURE(coord.z);
        REQUIRE(meshesB.count(coord) == 1);
        CHECK(mesh.opaque.positions == meshesB.at(coord).opaque.positions);
        CHECK(mesh.opaque.colors == meshesB.at(coord).opaque.colors);
        CHECK(a.chunk(coord)->blocks() == c.chunk(coord)->blocks());
    }
}

TEST_CASE("Streamer: a mesh requested before editing the chunk is discarded and the new one arrives") {
    TestBlocks b;
    std::atomic<int> generated{0};
    VoxelWorld world(b.registry, std::make_unique<CountingGenerator>(b.stone, generated));
    JobSystem jobs(2);
    ChunkStreamer streamer(world, smallSettings(), &jobs);
    std::map<ChunkCoord, ChunkMesh> meshes;
    converge(streamer, {0, 0, 0}, &meshes);
    const std::size_t before = meshes.at({0, 1, 0}).opaque.triangleCount();

    // Request a new mesh for chunk (0,1,0) and edit it again before receiving the result
    world.setBlock({5, 25, 5}, b.stone); // (0,1,0): y from 16 to 31
    streamer.update({0, 0, 0});          // Mesh requested (job in flight)
    world.setBlock({9, 25, 9}, b.stone); // Makes it stale
    converge(streamer, {0, 0, 0}, &meshes);
    CHECK(streamer.stats().discarded >= 1);
    // Final mesh includes both new blocks (each adds faces)
    CHECK(meshes.at({0, 1, 0}).opaque.triangleCount() > before + 12);
}

TEST_CASE("Streamer LOD: assigns LOD rings by horizontal Chebyshev distance") {
    TestBlocks b;
    std::atomic<int> generated{0};
    VoxelWorld world(b.registry, std::make_unique<CountingGenerator>(b.stone, generated));
    StreamingSettings s;
    s.lodRadius = {2, 4, 6};
    s.loadRadius = 7;
    s.meshRadius = 6;
    s.minChunkY = 0;
    s.maxChunkY = 0;
    s.maxResultsPerUpdate = 128;
    ChunkStreamer streamer(world, s);

    std::map<ChunkCoord, ChunkMesh> meshes;
    std::map<ChunkCoord, int> lods;
    converge(streamer, {0, 0, 0}, &meshes, &lods);

    CHECK(meshes.size() == 13 * 13); // Radius 6 -> (2*6+1)^2 = 169 chunks
    for (const auto& [coord, lod] : lods) {
        const int d = std::max(std::abs(coord.x), std::abs(coord.z));
        if (d <= 2) {
            CHECK(lod == 0);
        } else if (d <= 4) {
            CHECK(lod == 1);
        } else if (d <= 6) {
            CHECK(lod == 2);
        } else {
            FAIL("Chunk outside max radius");
        }
        CHECK(streamer.chunkLod(coord) == lod);
    }
}

TEST_CASE("Streamer LOD: 1-chunk hysteresis on ring transitions prevents flickering") {
    TestBlocks b;
    std::atomic<int> generated{0};
    VoxelWorld world(b.registry, std::make_unique<CountingGenerator>(b.stone, generated));
    StreamingSettings s;
    s.lodRadius = {2, 4, 0};
    s.loadRadius = 6;
    s.meshRadius = 4;
    s.minChunkY = 0;
    s.maxChunkY = 0;
    s.maxResultsPerUpdate = 128;
    ChunkStreamer streamer(world, s);

    std::map<ChunkCoord, ChunkMesh> meshes;
    std::map<ChunkCoord, int> lods;
    converge(streamer, {0, 0, 0}, &meshes, &lods);

    const ChunkCoord testChunk{2, 0, 0}; // d=2 from center (0,0) -> LOD 0
    REQUIRE(lods.count(testChunk) == 1);
    CHECK(lods.at(testChunk) == 0);

    // Move center to (-1, 0, 0): d = 3 = r0 + 1 -> stays at LOD 0 due to hysteresis
    converge(streamer, {-1, 0, 0}, &meshes, &lods);
    CHECK(streamer.chunkLod(testChunk) == 0);
    CHECK(lods.at(testChunk) == 0);

    // Move center to (-2, 0, 0): d = 4 > r0 + 1 -> transitions to LOD 1
    converge(streamer, {-2, 0, 0}, &meshes, &lods);
    CHECK(streamer.chunkLod(testChunk) == 1);
    CHECK(lods.at(testChunk) == 1);

    // Move center to (-1, 0, 0): d = 3 > r0 -> stays at LOD 1 (inward hysteresis)
    converge(streamer, {-1, 0, 0}, &meshes, &lods);
    CHECK(streamer.chunkLod(testChunk) == 1);
    CHECK(lods.at(testChunk) == 1);

    // Move center to (0, 0, 0): d = 2 <= r0 -> returns to LOD 0
    converge(streamer, {0, 0, 0}, &meshes, &lods);
    CHECK(streamer.chunkLod(testChunk) == 0);
    CHECK(lods.at(testChunk) == 0);
}

TEST_CASE("Streamer LOD: remeshing when changing LOD ring") {
    TestBlocks b;
    std::atomic<int> count{0};
    VoxelWorld world(b.registry, std::make_unique<CountingGenerator>(b.stone, count));
    StreamingSettings s;
    s.lodRadius = {1, 3, 0};
    s.loadRadius = 5;
    s.meshRadius = 3;
    s.minChunkY = 0;
    s.maxChunkY = 0;
    s.maxResultsPerUpdate = 128;
    ChunkStreamer streamer(world, s);

    std::map<ChunkCoord, ChunkMesh> meshes;
    std::map<ChunkCoord, int> lods;
    converge(streamer, {0, 0, 0}, &meshes, &lods);

    const ChunkCoord c{2, 0, 0}; // d=2 -> LOD 1
    CHECK(lods.at(c) == 1);

    // Move center to (2, 0, 0) -> now d=0 <= r0 -> remeshed at LOD 0
    converge(streamer, {2, 0, 0}, &meshes, &lods);
    CHECK(lods.at(c) == 0);
    CHECK(streamer.chunkLod(c) == 0);
}

TEST_CASE("Streamer LOD: discards stale-LOD meshes if center moves while meshing") {
    TestBlocks b;
    std::atomic<int> count{0};
    VoxelWorld world(b.registry, std::make_unique<CountingGenerator>(b.stone, count));
    JobSystem jobs(2);
    StreamingSettings s;
    s.lodRadius = {1, 3, 0};
    s.loadRadius = 6;
    s.meshRadius = 3;
    s.minChunkY = 0;
    s.maxChunkY = 0;
    s.maxResultsPerUpdate = 128;
    ChunkStreamer streamer(world, s, &jobs);

    std::map<ChunkCoord, ChunkMesh> meshes;
    std::map<ChunkCoord, int> lods;
    converge(streamer, {0, 0, 0}, &meshes, &lods);

    // Dirty a chunk at LOD 0 so it begins meshing
    const ChunkCoord c{0, 0, 0};
    world.setBlock(chunkOrigin(c) + glm::ivec3(2, 5, 2), b.stone);
    streamer.update(c); // Mesh requested at LOD 0

    // Before converging, move the center far away (e.g., to (10, 0, 0))
    // Chunk {0,0,0} is now out of radius or in a different LOD ring
    converge(streamer, {10, 0, 0}, &meshes, &lods);
    CHECK(streamer.stats().discarded >= 1);
}

namespace {
    StreamingSettings lodHoleSettings() {
        StreamingSettings s;
        s.lodRadius = {1, 3, 0};
        s.meshRadius = 3;
        s.loadRadius = 6;
        s.unloadMargin = 2;
        s.minChunkY = 0;
        s.maxChunkY = 0;
        s.maxResultsPerUpdate = 512;
        s.maxJobsInFlight = 512;
        return s;
    }

    void convergeJobs(ChunkStreamer& st, JobSystem& jobs, const ChunkCoord& c) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
        while (std::chrono::steady_clock::now() < deadline) {
            st.update(c);
            jobs.waitIdle();
            if (st.idle()) return;
        }
    }
} // namespace

TEST_CASE("Streamer LOD: initial mesh discarded by ring change is re-requested") {
    mge_test::TestBlocks blocks;
    VoxelWorld world(blocks.registry, std::make_unique<mge_test::FlatGenerator>(blocks.stone, 8));
    JobSystem jobs;
    ChunkStreamer st(world, lodHoleSettings(), &jobs);
    const ChunkCoord a{0, 0, 0}, b{1, 0, 0}, x{-3, 0, 0};
    convergeJobs(st, jobs, b); // X loaded (d = 4) but outside meshRadius (3)
    REQUIRE(world.isLoaded(x));
    REQUIRE(st.chunkLod(x) == -1);
    st.update(a);
    jobs.waitIdle(); // Requests mesh for X (LOD 1)
    st.update(b);    // Arrives with center at B: discarded
    convergeJobs(st, jobs, a);
    CHECK(st.chunkLod(x) == 1);
}

TEST_CASE("Streamer LOD: an edit whose mesh was discarded by ring change is remeshed") {
    mge_test::TestBlocks blocks;
    VoxelWorld world(blocks.registry, std::make_unique<mge_test::FlatGenerator>(blocks.stone, 8));
    JobSystem jobs;
    ChunkStreamer st(world, lodHoleSettings(), &jobs);
    const ChunkCoord a{0, 0, 0}, b{1, 0, 0}, x{-3, 0, 0};
    convergeJobs(st, jobs, a);
    REQUIRE(st.chunkLod(x) == 1);
    world.setBlock({-40, 10, 5}, blocks.stone); // Inside X
    st.update(a);
    jobs.waitIdle();
    st.update(b);
    bool remeshed = false;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
    while (std::chrono::steady_clock::now() < deadline) {
        StreamUpdate u = st.update(a);
        for (auto& m : u.meshes) {
            if (m.coord == x) remeshed = true;
        }
        jobs.waitIdle();
        if (st.idle()) break;
    }
    CHECK(remeshed);
}

TEST_CASE("Streamer LOD: setSettings (LOD off -> on) remeshes the rings") {
    mge_test::TestBlocks blocks;
    VoxelWorld world(blocks.registry, std::make_unique<mge_test::FlatGenerator>(blocks.stone, 8));
    JobSystem jobs;
    ChunkStreamer st(world, lodHoleSettings(), &jobs);
    const ChunkCoord a{0, 0, 0}, x{-3, 0, 0};
    convergeJobs(st, jobs, a);
    REQUIRE(st.chunkLod(x) == 1);
    StreamingSettings off = lodHoleSettings();
    off.lodRadius = {0, 0, 0};
    off.meshRadius = 1;
    st.setSettings(off);
    convergeJobs(st, jobs, a);
    st.setSettings(lodHoleSettings());
    bool delivered = false;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
    while (std::chrono::steady_clock::now() < deadline) {
        StreamUpdate u = st.update(a);
        for (auto& m : u.meshes) {
            if (m.coord == x) delivered = true;
        }
        jobs.waitIdle();
        if (st.idle()) break;
    }
    CHECK(delivered);
}

TEST_CASE("Streamer: onChunkInserted modifies the chunk before inserting and meshing it") {
    mge_test::TestBlocks blocks;
    VoxelWorld world(blocks.registry, std::make_unique<mge_test::FlatGenerator>(blocks.stone, 8));

    SUBCASE("Synchronous (single-threaded)") {
        StreamingSettings s = smallSettings();
        s.loadRadius = 1;
        s.meshRadius = 1;
        ChunkStreamer st(world, s, nullptr);

        int hookCalls = 0;
        st.onChunkInserted([&](const ChunkCoord& coord, ChunkData& data) {
            hookCalls++;
            if (coord == ChunkCoord{0, 0, 0}) {
                data.set(5, 5, 5, blocks.glass);
            }
        });

        st.update({0, 0, 0});
        CHECK(hookCalls > 0);
        CHECK(world.getBlock({5, 5, 5}) == blocks.glass);
    }

    SUBCASE("Asynchronous (with JobSystem)") {
        JobSystem jobs;
        StreamingSettings s = smallSettings();
        s.loadRadius = 1;
        s.meshRadius = 1;
        ChunkStreamer st(world, s, &jobs);

        st.onChunkInserted([&](const ChunkCoord& coord, ChunkData& data) {
            if (coord == ChunkCoord{0, 0, 0}) {
                data.set(3, 3, 3, blocks.glass);
            }
        });

        convergeJobs(st, jobs, {0, 0, 0});
        CHECK(world.getBlock({3, 3, 3}) == blocks.glass);
    }
}
