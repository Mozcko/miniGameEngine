// Headless smoke test: an Application WITHOUT modules (no window, no GPU, no SDL initialized)
// that runs 1000 frames generating and meshing sandbox chunks (mge_voxel) and checking invariants.
// It also travels in a straight line with a ChunkStreamer on the thread pool (#56, #57): memory must
// stay bounded (loaded chunks ≤ unload ring) and the worst main-thread update time is reported.
// Exits with code != 0 if anything fails. Runs in CI (including with ASan/UBSan).
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
            // Walks a simple spiral of columns, including negative ones; one column = all its chunks
            const int frame = static_cast<int>(getFrameCount());
            const int cx = (frame % 21) - 10;
            const int cz = (frame / 21 % 21) - 10;
            constexpr int kChunksY = sandbox::SandboxTerrain::kWorldChunksY;
            for (int cy = 0; cy < kChunksY; cy++) m_world.loadChunk({cx, cy, cz});

            const mge::ChunkData& bottom = *m_world.chunk({cx, 0, cz});
            for (int x = 0; x < mge::kChunkSize; x++) {
                for (int z = 0; z < mge::kChunkSize; z++) {
                    if (!m_registry.isSolid(bottom.get(x, 0, z))) fail("non-solid floor", cx, cz);
                    for (int cy = 0; cy < kChunksY; cy++) {
                        const mge::ChunkData& chunk = *m_world.chunk({cx, cy, cz});
                        for (int y = 0; y < mge::kChunkSize; y++) {
                            if (chunk.get(x, y, z) == m_blocks.water &&
                                cy * mge::kChunkSize + y > kWaterLevel)
                                fail("water above sea level", cx, cz);
                        }
                    }
                }
            }

            // CPU meshing (no GPU): valid meshes and the column has a surface
            std::size_t opaqueTriangles = 0;
            for (int cy = 0; cy < kChunksY; cy++) {
                const mge::ChunkMesh mesh = mge::buildChunkMesh(m_world, {cx, cy, cz});
                if (!mesh.opaque.validate().empty() || !mesh.transparent.validate().empty())
                    fail("invalid mesh", cx, cz);
                opaqueTriangles += mesh.opaque.triangleCount();
            }
            if (opaqueTriangles == 0) fail("column has no opaque faces", cx, cz);
            for (int cy = 0; cy < kChunksY; cy++) m_world.unloadChunk({cx, cy, cz}); // Bounded memory
            m_chunksChecked++;
        }

        // One chunk every 4 frames along +X: 250 chunks (4000 blocks) in 1000 frames
        void streamStep() {
            const auto start = std::chrono::steady_clock::now();
            const int x = static_cast<int>(getFrameCount() / 4);
            const mge::StreamUpdate update = m_streamer->update({x, 0, 0});
            m_streamedMeshes += update.meshes.size();
            const double ms =
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
            m_worstStreamMs = std::max(m_worstStreamMs, ms);
            if (m_streamWorld.chunkCount() > m_streamBound)
                fail("streaming: loaded chunks exceeded bound", x, 0);
        }

        void onShutdown() override {
            const mge::StreamingStats stats = m_streamer->stats();
            MGE_LOG_INFO("Smoke", "Streaming: ", m_streamedMeshes, " meshes, ", stats.loaded,
                         " loaded (limit ", m_streamBound, "), pool reused ",
                         m_streamWorld.reusedCount(), " times, discarded ", stats.discarded,
                         ", worst main-thread update ", m_worstStreamMs, " ms");
            if (m_streamedMeshes == 0) fail("streaming: no meshes received", 0, 0);
            m_streamer.reset();
            if (m_failures == 0) {
                MGE_LOG_INFO("Smoke", "OK: ", m_chunksChecked, " columns verified in ", getFrameCount(),
                             " frames without window or GPU");
            } else {
                MGE_LOG_ERROR("Smoke", m_failures, " failures");
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
            if (m_failures++ < 10) MGE_LOG_ERROR("Smoke", what, " in chunk (", cx, ", ", cz, ")");
            setExitCode(1);
        }

        mge::BlockRegistry m_registry;
        sandbox::SandboxBlocks m_blocks;
        mge::VoxelWorld m_world;
        mge::VoxelWorld m_streamWorld; // For the ChunkStreamer traversal
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
