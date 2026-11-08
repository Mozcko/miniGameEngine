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
    // acumula las mallas entregadas por chunk y opcionalmente su nivel de LOD
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

TEST_CASE("Streamer LOD: asigna anillos de LOD según la distancia horizontal de Chebyshev") {
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

    CHECK(meshes.size() == 13 * 13); // Radio 6 -> (2*6+1)^2 = 169 chunks
    for (const auto& [coord, lod] : lods) {
        const int d = std::max(std::abs(coord.x), std::abs(coord.z));
        if (d <= 2) {
            CHECK(lod == 0);
        } else if (d <= 4) {
            CHECK(lod == 1);
        } else if (d <= 6) {
            CHECK(lod == 2);
        } else {
            FAIL("Chunk fuera del radio maximo");
        }
        CHECK(streamer.chunkLod(coord) == lod);
    }
}

TEST_CASE("Streamer LOD: histeresis de 1 chunk en cambios de anillo evita parpadeo") {
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

    const ChunkCoord testChunk{2, 0, 0}; // d=2 al centro (0,0) -> LOD 0
    REQUIRE(lods.count(testChunk) == 1);
    CHECK(lods.at(testChunk) == 0);

    // Mover centro a (-1, 0, 0): d = 3 = r0 + 1 -> gracias a la histéresis permanece en LOD 0
    converge(streamer, {-1, 0, 0}, &meshes, &lods);
    CHECK(streamer.chunkLod(testChunk) == 0);
    CHECK(lods.at(testChunk) == 0);

    // Mover centro a (-2, 0, 0): d = 4 > r0 + 1 -> cambia a LOD 1
    converge(streamer, {-2, 0, 0}, &meshes, &lods);
    CHECK(streamer.chunkLod(testChunk) == 1);
    CHECK(lods.at(testChunk) == 1);

    // Mover centro a (-1, 0, 0): d = 3 > r0 -> permanece en LOD 1 (histéresis hacia adentro)
    converge(streamer, {-1, 0, 0}, &meshes, &lods);
    CHECK(streamer.chunkLod(testChunk) == 1);
    CHECK(lods.at(testChunk) == 1);

    // Mover centro a (0, 0, 0): d = 2 <= r0 -> vuelve a LOD 0
    converge(streamer, {0, 0, 0}, &meshes, &lods);
    CHECK(streamer.chunkLod(testChunk) == 0);
    CHECK(lods.at(testChunk) == 0);
}

TEST_CASE("Streamer LOD: re-mallado al cambiar de anillo de LOD") {
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

    // Mover centro a (2, 0, 0) -> ahora d=0 <= r0 -> se re-malla a LOD 0
    converge(streamer, {2, 0, 0}, &meshes, &lods);
    CHECK(lods.at(c) == 0);
    CHECK(streamer.chunkLod(c) == 0);
}

TEST_CASE("Streamer LOD: descartar mallas con LOD viejo si el centro se mueve mientras se malla") {
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

    // Ensuciar un chunk en LOD 0 para que empiece a mallarse
    const ChunkCoord c{0, 0, 0};
    world.setBlock(chunkOrigin(c) + glm::ivec3(2, 5, 2), b.stone);
    streamer.update(c); // Se pide la malla a LOD 0

    // Antes de converger, mover el centro muy lejos (por ej. a (10, 0, 0))
    // El chunk {0,0,0} ahora queda fuera de radio o en otro LOD
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

TEST_CASE("Streamer LOD: un primer mallado descartado por cambio de anillo se vuelve a pedir") {
    mge_test::TestBlocks blocks;
    VoxelWorld world(blocks.registry, std::make_unique<mge_test::FlatGenerator>(blocks.stone, 8));
    JobSystem jobs;
    ChunkStreamer st(world, lodHoleSettings(), &jobs);
    const ChunkCoord a{0, 0, 0}, b{1, 0, 0}, x{-3, 0, 0};
    convergeJobs(st, jobs, b); // X cargado (d = 4) pero fuera del radio de malla (3)
    REQUIRE(world.isLoaded(x));
    REQUIRE(st.chunkLod(x) == -1);
    st.update(a);
    jobs.waitIdle(); // Pide la malla de X (LOD 1)
    st.update(b);    // Llega con el centro en B: se descarta
    convergeJobs(st, jobs, a);
    CHECK(st.chunkLod(x) == 1);
}

TEST_CASE("Streamer LOD: una edición cuya malla se descartó por cambio de anillo se vuelve a mallar") {
    mge_test::TestBlocks blocks;
    VoxelWorld world(blocks.registry, std::make_unique<mge_test::FlatGenerator>(blocks.stone, 8));
    JobSystem jobs;
    ChunkStreamer st(world, lodHoleSettings(), &jobs);
    const ChunkCoord a{0, 0, 0}, b{1, 0, 0}, x{-3, 0, 0};
    convergeJobs(st, jobs, a);
    REQUIRE(st.chunkLod(x) == 1);
    world.setBlock({-40, 10, 5}, blocks.stone); // Dentro de X
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

TEST_CASE("Streamer LOD: setSettings (LOD off → on) vuelve a mallar los anillos") {
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
