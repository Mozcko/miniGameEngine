// Minimal 2D platformer (#45): loads ASCII levels from assets/levels, renders them with Renderer2D
// (colored rectangles, no textures), and plays sounds using the WAVs in assets/sounds. Serves to verify that
// the 2D path (pixel UI, input, audio, assets) keeps working; not intended to be a full game.
// A/D or arrows: move, Space/W/Up: jump, 1-4: level, S: synthesized/WAV sounds, F1: performance,
// Esc: quit.
#include <examples/platformer2d/PlatformerView.h>
#include <mge/mge.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <cstdlib>
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
            m_audio = getModule<mge::AudioModule>();
            // Two sound sets: generated in code (#48, default) and WAVs from assets/sounds
            // (#46). S toggles; PLATFORMER_SOUNDS=wav starts with the WAVs
            m_synthSounds = {m_audio->addSfx(mge::sfx::jump()), m_audio->addSfx(mge::sfx::coin()),
                             m_audio->addSfx(mge::sfx::hurt()), m_audio->addSfx(mge::sfx::powerUp())};
            m_wavSounds = {m_audio->loadWav("sounds/jump.wav"), m_audio->loadWav("sounds/coin.wav"),
                           m_audio->loadWav("sounds/hurt.wav"), m_audio->loadWav("sounds/power_up.wav")};
            const char* soundMode = std::getenv("PLATFORMER_SOUNDS");
            m_useWav = soundMode && std::string(soundMode) == "wav";
            // Sequencer music (#49), with a bit of delay. With WAVs there is no music (no
            // MP3)
            m_audio->setDelay({0.23f, 0.3f, 0.3f});
            m_theme = m_audio->addSong(mge::songs::platformerTheme());
            m_audio->setBusVolume(mge::AudioBus::Music, 0.8f);
            if (!m_useWav) m_audio->playSong(m_theme, 1.0f);
            m_gpu = getModule<mge::GpuModule>();
            mge::ActionMap& actions = m_platform->actions();
            actions.bindAxis("Move", mge::Key::D, mge::Key::A);
            actions.bindAxis("Move", mge::Key::Right, mge::Key::Left);
            actions.bind("Jump", mge::Key::Space);
            actions.bind("Jump", mge::Key::W);
            actions.bind("Jump", mge::Key::Up);
            actions.bind("Overlay", mge::Key::F1);
            actions.bind("ToggleSounds", mge::Key::S);
            actions.bind("Quit", mge::Key::Escape);
            const std::array<mge::Key, kLevelCount> levelKeys = {mge::Key::Num1, mge::Key::Num2,
                                                                 mge::Key::Num3, mge::Key::Num4};
            for (int i = 0; i < kLevelCount; i++) actions.bind("Level" + std::to_string(i + 1), levelKeys[i]);

            m_ui = std::make_unique<mge::Renderer2D>(m_gpu->device(), m_gpu->swapchainFormat());
            if (!m_ui->valid()) return false;
            m_overlay.setVisible(false);
            if (!loadLevel(0)) return false;
            MGE_LOG_INFO("Platformer",
                         "A/D: move, Space: jump, 1-4: level, S: sounds, F1: performance, Esc: quit");
            return true;
        }

        void onShutdown() override { m_ui.reset(); }

        void onUpdate(float /*dt*/) override {
            if (m_platform->wasActionPressed("Quit")) requestStop();
            if (m_platform->wasActionPressed("Overlay")) m_overlay.toggle();
            if (m_platform->wasActionPressed("ToggleSounds")) {
                m_useWav = !m_useWav;
                if (m_useWav)
                    m_audio->stopMusic(0.5f);
                else
                    m_audio->playSong(m_theme, 0.5f);
                MGE_LOG_INFO("Platformer",
                             m_useWav ? "Sounds: WAV (assets/sounds)" : "Sounds: generated in code");
            }
            for (int i = 0; i < kLevelCount; i++)
                if (m_platform->wasActionPressed("Level" + std::to_string(i + 1))) loadLevel(i);
        }

        void onFixedUpdate(float dt) override {
            platformer::PlayerInput input;
            input.move = m_platform->axis("Move");
            input.jump = m_platform->isActionDown("Jump");
            const platformer::StepEvents events = m_game->step(input, dt);
            const Sounds& sounds = m_useWav ? m_wavSounds : m_synthSounds;
            if (events.jumped) m_audio->play(sounds.jump, 0.5f);
            if (events.coins > 0) m_audio->play(sounds.coin);
            if (events.died) m_audio->play(sounds.hurt);
            if (events.finished) {
                m_audio->play(sounds.powerUp);
                loadLevel((m_levelIndex + 1) % kLevelCount);
            }
        }

        void onRender() override {
            const mge::GpuFrame* frame = m_gpu->frame();
            if (!frame) return;
            m_ui->begin(frame->width, frame->height, platformer::viewScale(m_game->level(), frame->height));
            platformer::drawScene(*m_ui, *m_game, m_levelIndex + 1); // Starts by filling the screen with sky
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
                MGE_LOG_ERROR("Platformer", "Level ", index + 1, ": ", error);
                return false;
            }
            MGE_LOG_INFO("Platformer", "Level ", index + 1, " (", level->biome, ", ", level->width, "x",
                         level->height, ")");
            m_game = std::make_unique<Platformer>(std::move(*level));
            m_levelIndex = index;
            return true;
        }

        static mge::ApplicationConfig makeConfig() {
            mge::ApplicationConfig config;
            config.title = "miniGameEngine - 2D Platformer";
            config.width = 1280;
            config.height = 720;
            return config;
        }

        mge::PlatformModule* m_platform = nullptr;
        mge::AudioModule* m_audio = nullptr;
        struct Sounds {
            mge::SampleId jump = mge::kInvalidSample;
            mge::SampleId coin = mge::kInvalidSample;
            mge::SampleId hurt = mge::kInvalidSample;
            mge::SampleId powerUp = mge::kInvalidSample;
        };
        Sounds m_synthSounds;
        Sounds m_wavSounds;
        bool m_useWav = false;
        mge::SongId m_theme = mge::kInvalidSong;
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
