// Smoke test headless: una Application SIN módulos (sin ventana, sin GPU, sin SDL inicializado)
// que corre 1000 frames generando y mallando chunks del sandbox (mge_voxel) y verificando invariantes.
// Además viaja en línea recta con un ChunkStreamer en el pool de hilos (#56, #57): la memoria tiene que
// quedar acotada (chunks cargados ≤ el anillo de descarga) y se reporta el peor update del hilo principal.
// Sale con código != 0 si algo falla. Se ejecuta en CI (también con ASan/UBSan).
#include <mge/mge.h>

#include <game/SandboxBlocks.h>
#include <game/SandboxTerrain.h>

#include <algorithm>
#include <chrono>
#include <memory>

namespace {
    constexpr std::uint64_t kFrames = 1000;
    constexpr int kWaterLevel = sandbox::SandboxTerrain::kWaterLevel;

    class HeadlessSmoke : public mge::Application {
    public:
        HeadlessSmoke()
            : mge::Application(makeConfig()), m_blocks(sandbox::registerSandboxBlocks(m_registry)),
              m_world(m_registry, std::make_unique<sandbox::SandboxTerrain>(m_blocks)),
              m_streamWorld(m_registry, std::make_unique<sandbox::SandboxTerrain>(m_blocks)) {}

    protected:
        bool onInit() override {
            mge::StreamingSettings settings;
            settings.loadRadius = 3;
            settings.meshRadius = 2;
            settings.unloadMargin = 2;
            settings.maxChunkY = sandbox::SandboxTerrain::kWorldChunksY - 1;
            settings.maxResultsPerUpdate = 8;
            m_streamer = std::make_unique<mge::ChunkStreamer>(m_streamWorld, settings, &jobs());
            const int side = 2 * (settings.loadRadius + settings.unloadMargin) + 1;
            m_streamBound = std::size_t(side) * side * sandbox::SandboxTerrain::kWorldChunksY;
            return true;
        }

        void onUpdate(float /*dt*/) override {
            streamStep();
            // Recorre una espiral simple de columnas, incluidas negativas; una columna = todos sus chunks
            const int frame = static_cast<int>(getFrameCount());
            const int cx = (frame % 21) - 10;
            const int cz = (frame / 21 % 21) - 10;
            constexpr int kChunksY = sandbox::SandboxTerrain::kWorldChunksY;
            for (int cy = 0; cy < kChunksY; cy++) m_world.loadChunk({cx, cy, cz});

            const mge::ChunkData& bottom = *m_world.chunk({cx, 0, cz});
            for (int x = 0; x < mge::kChunkSize; x++) {
                for (int z = 0; z < mge::kChunkSize; z++) {
                    if (!m_registry.isSolid(bottom.get(x, 0, z))) fail("fondo no sólido", cx, cz);
                    for (int cy = 0; cy < kChunksY; cy++) {
                        const mge::ChunkData& chunk = *m_world.chunk({cx, cy, cz});
                        for (int y = 0; y < mge::kChunkSize; y++) {
                            if (chunk.get(x, y, z) == m_blocks.water &&
                                cy * mge::kChunkSize + y > kWaterLevel)
                                fail("agua sobre el nivel del mar", cx, cz);
                        }
                    }
                }
            }

            // Mallado en CPU (sin GPU): mallas válidas y la columna tiene superficie
            std::size_t opaqueTriangles = 0;
            for (int cy = 0; cy < kChunksY; cy++) {
                const mge::ChunkMesh mesh = mge::buildChunkMesh(m_world, {cx, cy, cz});
                if (!mesh.opaque.validate().empty() || !mesh.transparent.validate().empty())
                    fail("malla inválida", cx, cz);
                opaqueTriangles += mesh.opaque.triangleCount();
            }
            if (opaqueTriangles == 0) fail("columna sin caras opacas", cx, cz);
            for (int cy = 0; cy < kChunksY; cy++) m_world.unloadChunk({cx, cy, cz}); // Memoria acotada
            m_chunksChecked++;
        }

        // Un chunk cada 4 frames en +X: 250 chunks (4000 bloques) en 1000 frames
        void streamStep() {
            const auto start = std::chrono::steady_clock::now();
            const int x = static_cast<int>(getFrameCount() / 4);
            const mge::StreamUpdate update = m_streamer->update({x, 0, 0});
            m_streamedMeshes += update.meshes.size();
            const double ms =
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
            m_worstStreamMs = std::max(m_worstStreamMs, ms);
            if (m_streamWorld.chunkCount() > m_streamBound)
                fail("streaming: más chunks cargados que el límite", x, 0);
        }

        void onShutdown() override {
            const mge::StreamingStats stats = m_streamer->stats();
            MGE_LOG_INFO("Smoke", "Streaming: ", m_streamedMeshes, " mallas, ", stats.loaded,
                         " cargados (límite ", m_streamBound, "), pool reutilizado ",
                         m_streamWorld.reusedCount(), " veces, descartados ", stats.discarded,
                         ", peor update del hilo principal ", m_worstStreamMs, " ms");
            if (m_streamedMeshes == 0) fail("streaming: no llegó ninguna malla", 0, 0);
            m_streamer.reset();
            if (m_failures == 0) {
                MGE_LOG_INFO("Smoke", "OK: ", m_chunksChecked, " columnas verificadas en ", getFrameCount(),
                             " frames sin ventana ni GPU");
            } else {
                MGE_LOG_ERROR("Smoke", m_failures, " fallos");
            }
        }

    private:
        static mge::ApplicationConfig makeConfig() {
            mge::ApplicationConfig config;
            config.title = "headless_smoke";
            config.headless = true;
            config.maxFrames = kFrames;
            return config;
        }

        void fail(const char* what, int cx, int cz) {
            if (m_failures++ < 10) MGE_LOG_ERROR("Smoke", what, " en el chunk (", cx, ", ", cz, ")");
            setExitCode(1);
        }

        mge::BlockRegistry m_registry;
        sandbox::SandboxBlocks m_blocks;
        mge::VoxelWorld m_world;
        mge::VoxelWorld m_streamWorld; // Para el viaje con ChunkStreamer
        std::unique_ptr<mge::ChunkStreamer> m_streamer;
        std::size_t m_streamBound = 0;
        std::size_t m_streamedMeshes = 0;
        double m_worstStreamMs = 0.0;
        int m_failures = 0;
        int m_chunksChecked = 0;
    };
}

mge::Application* mge::createApplication() {
    return new HeadlessSmoke();
}
