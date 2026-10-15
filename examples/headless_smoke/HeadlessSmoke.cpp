// Smoke test headless: una Application SIN módulos (sin ventana, sin GPU, sin SDL inicializado)
// que corre 1000 frames generando y mallando chunks del sandbox (mge_voxel) y verificando invariantes.
// Sale con código != 0 si algo falla. Se ejecuta en CI (también con ASan/UBSan).
#include <mge/mge.h>

#include <game/SandboxBlocks.h>
#include <game/SandboxTerrain.h>

#include <memory>

namespace {
    constexpr std::uint64_t kFrames = 1000;
    constexpr int kWaterLevel = sandbox::SandboxTerrain::kWaterLevel;

    class HeadlessSmoke : public mge::Application {
    public:
        HeadlessSmoke()
            : mge::Application(makeConfig()), m_blocks(sandbox::registerSandboxBlocks(m_registry)),
              m_world(m_registry, std::make_unique<sandbox::SandboxTerrain>(m_blocks)) {}

    protected:
        void onUpdate(float /*dt*/) override {
            // Recorre una espiral simple de coordenadas, incluidas negativas
            const int frame = static_cast<int>(getFrameCount());
            const int cx = (frame % 21) - 10;
            const int cz = (frame / 21 % 21) - 10;
            const mge::ChunkCoord coord{cx, 0, cz};
            const mge::ChunkData& chunk = m_world.loadChunk(coord);

            for (int x = 0; x < mge::kChunkSize; x++) {
                for (int z = 0; z < mge::kChunkSize; z++) {
                    if (!m_registry.isSolid(chunk.get(x, 0, z))) fail("fondo no sólido", cx, cz);
                    for (int y = 0; y < mge::kChunkSize; y++) {
                        if (chunk.get(x, y, z) == m_blocks.water && y > kWaterLevel)
                            fail("agua sobre el nivel del mar", cx, cz);
                    }
                }
            }

            // Mallado en CPU (sin GPU): la malla es válida y el chunk tiene superficie
            const mge::ChunkMesh mesh = mge::buildChunkMesh(m_world, coord);
            if (!mesh.opaque.validate().empty() || !mesh.transparent.validate().empty())
                fail("malla inválida", cx, cz);
            if (mesh.opaque.empty()) fail("chunk sin caras opacas", cx, cz);
            m_world.unloadChunk(coord); // Memoria acotada
            m_chunksChecked++;
        }

        void onShutdown() override {
            if (m_failures == 0) {
                MGE_LOG_INFO("Smoke", "OK: ", m_chunksChecked, " chunks verificados en ", getFrameCount(),
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
        int m_failures = 0;
        int m_chunksChecked = 0;
    };
}

mge::Application* mge::createApplication() {
    return new HeadlessSmoke();
}
