// ChunkStreamer (#57): anillos con histéresis, presupuesto de memoria, pool, resultados viejos, con y sin
// hilos
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
    // Suelo plano de 20 bloques que cuenta cuántas veces se generó cada chunk
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

    // update hasta que no quede nada por hacer (con hilos, esperando a que terminen los trabajos);
    // acumula las mallas entregadas por chunk
    void converge(ChunkStreamer& streamer, const ChunkCoord& center,
                  std::map<ChunkCoord, ChunkMesh>* meshes = nullptr) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
        while (std::chrono::steady_clock::now() < deadline) {
            StreamUpdate u = streamer.update(center);
            if (meshes) {
                for (auto& [c, m] : u.meshes) (*meshes)[c] = std::move(m);
                for (const ChunkCoord& c : u.unloaded) meshes->erase(c);
            }
            if (streamer.idle()) return;
            const StreamingStats stats = streamer.stats();
            if (stats.pendingGenerate + stats.pendingMesh > 0)
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        FAIL("El streamer no terminó en 30 s");
    }
}

TEST_CASE("Streamer: carga el radio, malla el radio de malla y queda ocioso") {
    TestBlocks b;
    std::atomic<int> generated{0};
    VoxelWorld world(b.registry, std::make_unique<CountingGenerator>(b.stone, generated));
    ChunkStreamer streamer(world, smallSettings());
    std::map<ChunkCoord, ChunkMesh> meshes;
    converge(streamer, {0, 0, 0}, &meshes);
    CHECK(world.chunkCount() == 7 * 7 * 2); // Radio 3, 2 de alto
    CHECK(meshes.size() == 5 * 5 * 2);      // Radio de malla 2
    CHECK(generated.load() == 7 * 7 * 2);
    CHECK(world.dirtyChunks().size() == 7 * 7 * 2 - 5 * 5 * 2); // Solo quedan sin mallar los de afuera
}

TEST_CASE("Streamer: la memoria queda acotada al viajar 2000 bloques; el pool se reutiliza") {
    TestBlocks b;
    std::atomic<int> generated{0};
    VoxelWorld world(b.registry, std::make_unique<CountingGenerator>(b.stone, generated));
    const StreamingSettings settings = smallSettings();
    ChunkStreamer streamer(world, settings);
    const int unloadSide = 2 * (settings.loadRadius + settings.unloadMargin) + 1;
    const std::size_t bound = std::size_t(unloadSide) * unloadSide * 2;
    std::size_t peak = 0;
    for (int x = 0; x <= 125; x++) { // 125 chunks = 2000 bloques
        for (int i = 0; i < 3; i++) streamer.update({x, 0, 0});
        peak = std::max(peak, world.chunkCount());
    }
    CAPTURE(peak);
    CHECK(peak <= bound);
    CHECK(world.reusedCount() > 0); // Las cargas aprovecharon chunks descargados
    CHECK(world.isLoaded({125, 0, 0}));
    CHECK_FALSE(world.isLoaded({0, 0, 0}));
}

TEST_CASE("Streamer: histéresis, oscilar sobre un borde no descarga ni regenera") {
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

TEST_CASE("Streamer: presupuesto de memoria (maxLoadedChunks)") {
    TestBlocks b;
    std::atomic<int> generated{0};
    VoxelWorld world(b.registry, std::make_unique<CountingGenerator>(b.stone, generated));
    StreamingSettings settings = smallSettings();
    settings.maxLoadedChunks = 60; // Menos que 7×7×2 = 98
    ChunkStreamer streamer(world, settings);
    for (int i = 0; i < 200; i++) {
        streamer.update({0, 0, 0});
        CHECK(world.chunkCount() <= 60);
    }
    // Lo cercano (radio de malla 2: 5×5×2 = 50) entra completo
    for (int x = -2; x <= 2; x++)
        for (int z = -2; z <= 2; z++) CHECK(world.isLoaded({x, 0, z}));
}

TEST_CASE("Streamer: con JobSystem da el mismo mundo y las mismas mallas que sin hilos") {
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

TEST_CASE("Streamer: una malla pedida antes de editar el chunk se descarta y llega la nueva") {
    TestBlocks b;
    std::atomic<int> generated{0};
    VoxelWorld world(b.registry, std::make_unique<CountingGenerator>(b.stone, generated));
    JobSystem jobs(2);
    ChunkStreamer streamer(world, smallSettings(), &jobs);
    std::map<ChunkCoord, ChunkMesh> meshes;
    converge(streamer, {0, 0, 0}, &meshes);
    const std::size_t before = meshes.at({0, 1, 0}).opaque.triangleCount();

    // Pedir la malla nueva del chunk (0,1,0) y editarlo otra vez antes de recibirla
    world.setBlock({5, 25, 5}, b.stone); // (0,1,0): y de 16 a 31
    streamer.update({0, 0, 0});          // Se pide la malla (job en curso)
    world.setBlock({9, 25, 9}, b.stone); // La vuelve vieja
    converge(streamer, {0, 0, 0}, &meshes);
    CHECK(streamer.stats().discarded >= 1);
    // La malla final incluye los dos bloques nuevos (cada uno suma caras)
    CHECK(meshes.at({0, 1, 0}).opaque.triangleCount() > before + 12);
}
