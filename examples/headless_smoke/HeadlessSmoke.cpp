// Smoke test headless: una Application SIN módulos (sin ventana, sin GPU, sin SDL inicializado)
// que corre 1000 frames generando chunks del sandbox y verificando sus invariantes.
// Sale con código != 0 si algo falla. Se ejecuta en CI (también con ASan/UBSan).
#include <mge/core/Application.h>
#include <mge/core/Log.h>
#include <sandbox/Chunk.h>

#include <memory>

namespace {
    constexpr std::uint64_t kFrames = 1000;
    constexpr int kWaterLevel = 5; // Igual que Chunk::generate

    class HeadlessSmoke : public mge::Application {
    public:
        HeadlessSmoke() : mge::Application(makeConfig()) {}

    protected:
        void onUpdate(float /*dt*/) override {
            // Recorre una espiral simple de coordenadas, incluidas negativas
            const int frame = static_cast<int>(getFrameCount());
            const int cx = (frame % 21) - 10;
            const int cz = (frame / 21 % 21) - 10;
            auto chunk = std::make_unique<mge::Chunk>(cx, cz);

            for (int x = 0; x < mge::kChunkSize; x++) {
                for (int z = 0; z < mge::kChunkSize; z++) {
                    if (!chunk->getBlock(x, 0, z).isSolid()) fail("fondo no sólido", cx, cz);
                    for (int y = 0; y < mge::kChunkHeight; y++) {
                        if (chunk->getBlock(x, y, z).getType() == mge::BlockType::Water && y > kWaterLevel)
                            fail("agua sobre el nivel del mar", cx, cz);
                    }
                }
            }
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

        int m_failures = 0;
        int m_chunksChecked = 0;
    };
}

mge::Application* mge::createApplication() {
    return new HeadlessSmoke();
}
