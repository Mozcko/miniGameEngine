#include <mge/mge.h>

#include "SdfGlideLogic.h"
#include <shaders/glide_frag.h>

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <array>
#include <memory>
#include <string>

namespace {

    class SdfGlideApp : public mge::Application {
    public:
        SdfGlideApp() : mge::Application(makeConfig()) {
            addModule<mge::PlatformModule>();
            addModule<mge::GpuModule>();
            addModule<mge::AudioModule>();
        }

    protected:
        bool onInit() override {
            m_platform = getModule<mge::PlatformModule>();
            m_gpu = getModule<mge::GpuModule>();
            m_audio = getModule<mge::AudioModule>();
            if (!m_platform || !m_gpu || !m_audio) return false;

            // 1. Controles
            auto& actions = m_platform->actions();
            actions.bindAxis("SteerX", mge::Key::D, mge::Key::A);
            actions.bindAxis("SteerY", mge::Key::W, mge::Key::S);
            actions.bind("Boost", mge::Key::Space);
            actions.bind("Restart", mge::Key::R);
            actions.bind("Quit", mge::Key::Escape);

            m_platform->setCursorLocked(true);

            // 2. Renderizadores (Renderer rasterizado + SDF raymarching pass + UI 2D)
            mge::GpuDevice& device = m_gpu->device();
            m_renderer = std::make_unique<mge::MeshRenderer>(device, m_gpu->swapchainFormat());
            m_sdfRenderer = std::make_unique<mge::SdfRenderer>(
                device, m_gpu->swapchainFormat(), device.depthFormat(), mge::shaders::kGlideFrag);
            m_ui = std::make_unique<mge::Renderer2D>(device, m_gpu->swapchainFormat());

            if (!m_renderer || !m_sdfRenderer || !m_sdfRenderer->valid() || !m_ui || !m_ui->valid()) {
                MGE_LOG_ERROR("SdfGlide", "Fallo al inicializar los renderizadores GPU");
                return false;
            }

            // 3. Mallas rasterizadas: orbe del jugador y motes de luz
            const mge::MeshData orbData =
                mge::primitives::uvSphere(0.32f, 16, 16, glm::vec4(1.0f, 0.85f, 0.30f, 1.0f));
            const mge::MeshData moteData =
                mge::primitives::uvSphere(0.18f, 12, 12, glm::vec4(0.30f, 0.85f, 1.0f, 1.0f));

            m_orbMesh = device.uploadMesh(orbData);
            m_moteMesh = device.uploadMesh(moteData);

            // 4. Iluminación atmosférica del cañón
            m_lighting.sunDirection = glm::normalize(glm::vec3(-0.3f, -1.0f, -0.4f));
            m_lighting.sunColor = glm::vec3(1.0f, 0.92f, 0.85f);
            m_lighting.skyAmbient = glm::vec3(0.50f, 0.45f, 0.55f);
            m_lighting.groundAmbient = glm::vec3(0.25f, 0.18f, 0.15f);
            m_lighting.fog = true;
            m_lighting.fogColor = glm::vec3(0.65f, 0.40f, 0.30f);
            m_lighting.fogStart = 40.0f;
            m_lighting.fogEnd = 160.0f;

            // 5. Audio procedural sintetizado
            m_audioParams = sdf_glide::GlideAudioParams::create();
            for (std::size_t i = 0; i < m_audioParams.moteSounds.size(); ++i) {
                m_moteSampleIds[i] = m_audio->addSfx(m_audioParams.moteSounds[i]);
            }
            m_hitSampleId = m_audio->addSfx(m_audioParams.hitSound);
            m_boostSampleId = m_audio->addSfx(m_audioParams.boostSound);

            m_themeSongId = m_audio->addSong(m_audioParams.themeSong);
            m_audio->setDelay({0.30f, 0.35f, 0.30f});
            m_audio->setBusVolume(mge::AudioBus::Music, 0.65f);
            m_audio->playSong(m_themeSongId, 1.0f);

            MGE_LOG_INFO("SdfGlide",
                         "SDF Glide iniciado. WASD/Raton: virar, Espacio: boost, R: reiniciar, Esc: salir");
            return true;
        }

        void onUpdate(float /*dt*/) override {
            if (m_platform->wasActionPressed("Quit")) {
                requestStop();
                return;
            }

            // Lectura continua de controles
            m_pendingInput.steerX = m_platform->axis("SteerX");
            m_pendingInput.steerY = m_platform->axis("SteerY");

            if (m_platform->isCursorLocked()) {
                const auto mouseDelta = m_platform->input().mouseDelta();
                constexpr float kMouseSensitivity = 0.05f;
                m_pendingInput.steerX += mouseDelta.x * kMouseSensitivity;
                m_pendingInput.steerY += -mouseDelta.y * kMouseSensitivity;
            }
            m_pendingInput.steerX = glm::clamp(m_pendingInput.steerX, -1.0f, 1.0f);
            m_pendingInput.steerY = glm::clamp(m_pendingInput.steerY, -1.0f, 1.0f);
            m_pendingInput.boost = m_platform->isActionDown("Boost");

            // Enganche de acciones de borde (edge actions) para el paso fijo
            if (m_platform->wasActionPressed("Restart")) {
                m_latchedRestart = true;
            }
            if (m_platform->wasActionPressed("Boost")) {
                m_latchedBoostStart = true;
            }
        }

        void onFixedUpdate(float fixedDt) override {
            sdf_glide::GlideInput input = m_pendingInput;
            if (m_latchedRestart) {
                input.restart = true;
                m_latchedRestart = false;
            }
            if (m_latchedBoostStart) {
                input.boost = true;
                m_latchedBoostStart = false;
            }

            m_game.update(fixedDt, input);

            // Procesar eventos sonoros ocurridos durante la simulación
            for (const auto event : m_game.audioEvents()) {
                switch (event) {
                case sdf_glide::AudioEvent::CollectMote: {
                    const int comboIdx = std::clamp(m_game.combo() - 1, 0, 7);
                    m_audio->play(m_moteSampleIds[static_cast<size_t>(comboIdx)], 0.45f);
                    break;
                }
                case sdf_glide::AudioEvent::HitWall: m_audio->play(m_hitSampleId, 0.65f); break;
                case sdf_glide::AudioEvent::BoostStart: m_audio->play(m_boostSampleId, 0.40f); break;
                case sdf_glide::AudioEvent::GameOver: m_audio->play(m_hitSampleId, 0.80f); break;
                case sdf_glide::AudioEvent::None: break;
                }
            }
        }

        void onRender() override {
            const mge::GpuFrame* frame = m_gpu->frame();
            if (!frame) return;

            const float aspect = float(frame->width) / float(std::max(1u, frame->height));

            // Cámara en tercera persona siguiendo al orbe (interpolada entre pasos de simulación)
            const float alpha = time().alpha();
            const glm::vec3 renderPos = glm::mix(m_game.previousPosition(), m_game.playerPosition(), alpha);
            const glm::vec3 eye = renderPos + glm::vec3(0.0f, 1.2f, 3.8f);
            const glm::vec3 target = renderPos + glm::vec3(0.0f, 0.2f, -8.0f);
            const glm::vec3 up(0.0f, 1.0f, 0.0f);

            const glm::mat4 view = glm::lookAt(eye, target, up);
            const glm::mat4 proj = glm::perspectiveRH_ZO(glm::radians(60.0f), aspect, 0.1f, 250.0f);
            const glm::mat4 viewProj = proj * view;
            const glm::mat4 invViewProj = glm::inverse(viewProj);

            // 1. Pase 3D rasterizado (orbe y motes)
            if (!m_renderer->begin(*frame->cmd, frame->swapchain, frame->width, frame->height,
                                   glm::vec4(m_lighting.fogColor, 1.0f))) {
                return;
            }
            m_renderer->setCamera(viewProj, eye);
            m_renderer->setLighting(m_lighting);

            // A. Dibujar orbe del jugador (titila si está en periodo de invulnerabilidad)
            const bool visible =
                (m_game.invulnerableTimer() <= 0.0f) || (int(m_game.invulnerableTimer() * 15.0f) % 2 == 0);
            if (visible && !m_game.isGameOver()) {
                const glm::mat4 orbModel = glm::translate(glm::mat4(1.0f), renderPos);
                m_renderer->draw(m_orbMesh, orbModel);
            }

            // B. Dibujar motes de luz activos dentro del rango visual
            for (const auto& mote : m_game.motes()) {
                if (mote.collected) continue;
                if (mote.position.z > eye.z + 5.0f || mote.position.z < renderPos.z - 120.0f) continue;
                const glm::mat4 moteModel = glm::translate(glm::mat4(1.0f), mote.position);
                m_renderer->draw(m_moteMesh, moteModel);
            }

            // 2. Pase SDF Raymarching (Cañón procedural, se mezcla por profundidad con los orbes)
            mge::SdfFrame sdfFrame;
            sdfFrame.viewProjection = viewProj;
            sdfFrame.inverseViewProjection = invViewProj;
            sdfFrame.cameraPosition = eye;
            sdfFrame.time = static_cast<float>(time().unscaledElapsed());
            sdfFrame.params[0] = glm::vec4(renderPos, 1.0f); // Luz del orbe hacia las paredes

            m_sdfRenderer->draw(*m_renderer, sdfFrame, m_lighting);
            m_renderer->end();

            // 3. HUD 2D (Renderer2D con fuente integrada)
            m_ui->begin(frame->width, frame->height, 1.0f);

            const int score = m_game.score();
            const int best = m_game.bestScore();
            const int shields = m_game.shields();
            const int combo = m_game.combo();
            const int distMeters = static_cast<int>(m_game.distanceTraveled());

            // Panel superior izquierdo
            std::string scoreStr = "SCORE: " + std::to_string(score);
            if (combo > 1) scoreStr += "  (x" + std::to_string(combo) + ")";
            m_ui->text({20.0f, 20.0f}, scoreStr, glm::vec4(1.0f), 2.0f);

            std::string shieldStr = "SHIELDS: ";
            for (int s = 0; s < shields; ++s) shieldStr += "[#] ";
            for (int s = shields; s < sdf_glide::GlideGame::kMaxShields; ++s) shieldStr += "[ ] ";
            m_ui->text({20.0f, 45.0f}, shieldStr,
                       shields > 1 ? glm::vec4(0.3f, 1.0f, 0.4f, 1.0f) : glm::vec4(1.0f, 0.3f, 0.3f, 1.0f),
                       2.0f);

            // Panel superior derecho
            const std::string bestStr = "BEST: " + std::to_string(best);
            const std::string distStr = "DIST: " + std::to_string(distMeters) + " m";
            m_ui->text({float(frame->width) - 200.0f, 20.0f}, bestStr, glm::vec4(0.9f, 0.9f, 0.5f, 1.0f),
                       2.0f);
            m_ui->text({float(frame->width) - 200.0f, 45.0f}, distStr, glm::vec4(0.8f, 0.8f, 0.8f, 1.0f),
                       2.0f);

            // Barra de energía (inferior central)
            constexpr float kBarW = 240.0f;
            constexpr float kBarH = 14.0f;
            const float barX = (float(frame->width) - kBarW) * 0.5f;
            const float barY = float(frame->height) - 40.0f;

            m_ui->text({barX, barY - 18.0f}, "BOOST ENERGY", glm::vec4(0.8f, 0.9f, 1.0f, 1.0f), 1.5f);
            m_ui->rect({barX, barY}, {kBarW, kBarH}, glm::vec4(0.15f, 0.15f, 0.20f, 0.8f));
            const float energyRatio = m_game.energy() / sdf_glide::GlideGame::kMaxEnergy;
            const glm::vec4 energyColor =
                m_game.isBoosting() ? glm::vec4(1.0f, 0.6f, 0.2f, 1.0f) : glm::vec4(0.2f, 0.7f, 1.0f, 1.0f);
            m_ui->rect({barX, barY}, {kBarW * energyRatio, kBarH}, energyColor);
            m_ui->rectOutline({barX, barY}, {kBarW, kBarH}, glm::vec4(0.8f, 0.8f, 0.9f, 1.0f));

            // Cartel de fin de partida
            if (m_game.isGameOver()) {
                const float centerX = float(frame->width) * 0.5f;
                const float centerY = float(frame->height) * 0.5f;
                m_ui->rect({centerX - 180.0f, centerY - 60.0f}, {360.0f, 120.0f},
                           glm::vec4(0.05f, 0.05f, 0.08f, 0.90f));
                m_ui->rectOutline({centerX - 180.0f, centerY - 60.0f}, {360.0f, 120.0f},
                                  glm::vec4(0.9f, 0.3f, 0.3f, 1.0f));
                m_ui->text({centerX - 70.0f, centerY - 40.0f}, "GAME OVER", glm::vec4(1.0f, 0.3f, 0.3f, 1.0f),
                           3.0f);
                m_ui->text({centerX - 120.0f, centerY + 10.0f}, "PRESS 'R' TO RESTART", glm::vec4(1.0f),
                           2.0f);
            }

            m_ui->flush(*frame->cmd, frame->swapchain);
        }

        void onShutdown() override {
            m_orbMesh = {};
            m_moteMesh = {};
            m_ui.reset();
            m_sdfRenderer.reset();
            m_renderer.reset();

            MGE_LOG_INFO("SdfGlide", "SDF Glide cerrado");
        }

    private:
        static mge::ApplicationConfig makeConfig() {
            mge::ApplicationConfig config;
            config.title = "miniGameEngine - SDF Glide";
            config.width = 1280;
            config.height = 720;
            return config;
        }

        mge::PlatformModule* m_platform = nullptr;
        mge::GpuModule* m_gpu = nullptr;
        mge::AudioModule* m_audio = nullptr;

        std::unique_ptr<mge::MeshRenderer> m_renderer;
        std::unique_ptr<mge::SdfRenderer> m_sdfRenderer;
        std::unique_ptr<mge::Renderer2D> m_ui;

        mge::GpuMesh m_orbMesh;
        mge::GpuMesh m_moteMesh;
        mge::Lighting m_lighting;

        sdf_glide::GlideGame m_game;
        sdf_glide::GlideInput m_pendingInput;
        bool m_latchedRestart = false;
        bool m_latchedBoostStart = false;
        sdf_glide::GlideAudioParams m_audioParams;
        std::array<mge::SampleId, 8> m_moteSampleIds{};
        mge::SampleId m_hitSampleId = mge::kInvalidSample;
        mge::SampleId m_boostSampleId = mge::kInvalidSample;
        mge::SongId m_themeSongId = mge::kInvalidSong;
    };

} // namespace

mge::Application* mge::createApplication() {
    return new SdfGlideApp();
}
