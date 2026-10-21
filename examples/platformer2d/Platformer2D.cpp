// Plataformero 2D mínimo (#45): carga los niveles ASCII de assets/levels, los dibuja con Renderer2D
// (rectángulos de colores, sin texturas) y suena con los WAV de assets/sounds. Sirve para probar que el
// camino 2D (UI en píxeles, input, audio, assets) sigue funcionando; no pretende ser un juego completo.
// A/D o flechas: mover, Espacio/W/arriba: saltar, 1-4: nivel, F1: rendimiento, Esc: salir.
#include <examples/platformer2d/PlatformerView.h>
#include <mge/mge.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <string>

namespace {
    using platformer::Level;
    using platformer::Platformer;

    constexpr int kLevelCount = 4;

    class Platformer2D : public mge::Application {
    public:
        Platformer2D() : mge::Application(makeConfig()) {
            addModule<mge::PlatformModule>();
            addModule<mge::GpuModule>();
            addModule<mge::AudioModule>();
        }

    protected:
        bool onInit() override {
            m_platform = getModule<mge::PlatformModule>();
            m_gpu = getModule<mge::GpuModule>();
            mge::ActionMap& actions = m_platform->actions();
            actions.bindAxis("Move", mge::Key::D, mge::Key::A);
            actions.bindAxis("Move", mge::Key::Right, mge::Key::Left);
            actions.bind("Jump", mge::Key::Space);
            actions.bind("Jump", mge::Key::W);
            actions.bind("Jump", mge::Key::Up);
            actions.bind("Overlay", mge::Key::F1);
            actions.bind("Quit", mge::Key::Escape);
            const std::array<mge::Key, kLevelCount> levelKeys = {mge::Key::Num1, mge::Key::Num2,
                                                                 mge::Key::Num3, mge::Key::Num4};
            for (int i = 0; i < kLevelCount; i++) actions.bind("Level" + std::to_string(i + 1), levelKeys[i]);

            m_ui = std::make_unique<mge::Renderer2D>(m_gpu->device(), m_gpu->swapchainFormat());
            if (!m_ui->valid()) return false;
            m_overlay.setVisible(false);
            if (!loadLevel(0)) return false;
            MGE_LOG_INFO("Platformer",
                         "A/D: mover, Espacio: saltar, 1-4: nivel, F1: rendimiento, Esc: salir");
            return true;
        }

        void onShutdown() override { m_ui.reset(); }

        void onUpdate(float /*dt*/) override {
            if (m_platform->wasActionPressed("Quit")) requestStop();
            if (m_platform->wasActionPressed("Overlay")) m_overlay.toggle();
            for (int i = 0; i < kLevelCount; i++)
                if (m_platform->wasActionPressed("Level" + std::to_string(i + 1))) loadLevel(i);
        }

        void onFixedUpdate(float dt) override {
            platformer::PlayerInput input;
            input.move = m_platform->axis("Move");
            input.jump = m_platform->isActionDown("Jump");
            const platformer::StepEvents events = m_game->step(input, dt);
            if (events.jumped) playSound("jump.wav", 64);
            if (events.coins > 0) playSound("coin.wav");
            if (events.died) playSound("hurt.wav");
            if (events.finished) {
                playSound("power_up.wav");
                loadLevel((m_levelIndex + 1) % kLevelCount);
            }
        }

        void onRender() override {
            const mge::GpuFrame* frame = m_gpu->frame();
            if (!frame) return;
            m_ui->begin(frame->width, frame->height, platformer::viewScale(m_game->level(), frame->height));
            platformer::drawScene(*m_ui, *m_game, m_levelIndex + 1); // Empieza tapando todo con el cielo
            m_overlay.addFrame(time().unscaledDelta() * 1000.0f, frame->acquireMs);
            m_overlay.draw(*m_ui, {8.0f, 28.0f});
            m_ui->flush(*frame->cmd, frame->swapchain);
        }

    private:
        bool loadLevel(int index) {
            std::string error;
            std::optional<Level> level =
                platformer::loadLevel("levels/level" + std::to_string(index + 1) + ".txt", &error);
            if (!level) {
                MGE_LOG_ERROR("Platformer", "Nivel ", index + 1, ": ", error);
                return false;
            }
            MGE_LOG_INFO("Platformer", "Nivel ", index + 1, " (", level->biome, ", ", level->width, "x",
                         level->height, ")");
            m_game = std::make_unique<Platformer>(std::move(*level));
            m_levelIndex = index;
            return true;
        }

        static void playSound(const std::string& name, int volume = 128) {
            mge::AudioManager::playSound(mge::assetPath("sounds/" + name).string(), volume);
        }

        static mge::ApplicationConfig makeConfig() {
            mge::ApplicationConfig config;
            config.title = "miniGameEngine - Plataformero 2D";
            config.width = 1280;
            config.height = 720;
            return config;
        }

        mge::PlatformModule* m_platform = nullptr;
        mge::GpuModule* m_gpu = nullptr;
        std::unique_ptr<mge::Renderer2D> m_ui;
        mge::DebugOverlay m_overlay;
        std::unique_ptr<Platformer> m_game;
        int m_levelIndex = 0;
    };
}

mge::Application* mge::createApplication() {
    return new Platformer2D();
}
