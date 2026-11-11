#include <mge/mge.h>

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace {

    struct Gem {
        glm::vec3 position{0.0f};
        float baseHeight = 0.8f;
        float bobPhase = 0.0f;
    };

    struct Hazard {
        glm::vec3 position{0.0f};
        float radius = 0.8f;
    };

    class GuideGame : public mge::Application {
    public:
        GuideGame() : mge::Application(makeConfig()) {
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

            // 1. Acciones de entrada desacopladas del teclado
            auto& actions = m_platform->actions();
            actions.bindAxis("MoveX", mge::Key::D, mge::Key::A);
            actions.bindAxis("MoveZ", mge::Key::S, mge::Key::W);
            actions.bind("Jump", mge::Key::Space);
            actions.bind("Restart", mge::Key::R);
            actions.bind("Quit", mge::Key::Escape);

            // 2. Renderizadores (MeshRenderer 3D y Renderer2D para HUD)
            mge::GpuDevice& device = m_gpu->device();
            m_renderer = std::make_unique<mge::MeshRenderer>(device, m_gpu->swapchainFormat());
            m_ui = std::make_unique<mge::Renderer2D>(device, m_gpu->swapchainFormat());

            if (!m_renderer || !m_ui || !m_ui->valid()) {
                MGE_LOG_ERROR("GuideGame", "Fallo al inicializar los renderizadores GPU");
                return false;
            }

            // 3. Mallas geométricas generadas en código puro (cero archivos de assets)
            m_floorMesh = device.uploadMesh(
                mge::primitives::plane(36.0f, 36.0f, 6, 6, glm::vec4(0.22f, 0.28f, 0.35f, 1.0f)));
            m_wallMesh = device.uploadMesh(
                mge::primitives::box(glm::vec3(1.0f, 2.5f, 1.0f), glm::vec4(0.38f, 0.44f, 0.52f, 1.0f)));
            m_playerMesh = device.uploadMesh(
                mge::primitives::uvSphere(0.6f, 24, 16, glm::vec4(0.25f, 0.85f, 1.0f, 1.0f)));
            m_gemMesh =
                device.uploadMesh(mge::primitives::icosphere(0.42f, 1, glm::vec4(1.0f, 0.85f, 0.20f, 1.0f)));
            m_hazardMesh = device.uploadMesh(
                mge::primitives::cone(0.65f, 1.2f, 16, glm::vec4(0.95f, 0.25f, 0.25f, 1.0f)));

            // 4. Iluminación atmosférica (sol, hemisferio y niebla)
            m_lighting.sunDirection = glm::normalize(glm::vec3(-0.4f, -1.0f, -0.3f));
            m_lighting.sunColor = glm::vec3(1.0f, 0.95f, 0.85f);
            m_lighting.skyAmbient = glm::vec3(0.40f, 0.45f, 0.55f);
            m_lighting.groundAmbient = glm::vec3(0.20f, 0.18f, 0.16f);
            m_lighting.fog = true;
            m_lighting.fogColor = glm::vec3(0.10f, 0.12f, 0.16f);
            m_lighting.fogStart = 20.0f;
            m_lighting.fogEnd = 50.0f;

            // 5. Audio procedural sintetizado
            m_coinSfx = m_audio->addSfx(mge::sfx::coin());
            m_jumpSfx = m_audio->addSfx(mge::sfx::jump());
            m_hurtSfx = m_audio->addSfx(mge::sfx::hurt());

            mge::Song song(132.0f, 4);
            mge::Instrument lead;
            lead.wave = mge::Waveform::Square;
            lead.pulseWidth = 0.35f;
            lead.envelope = {0.005f, 0.05f, 0.35f, 0.10f};
            lead.gain = 0.20f;

            mge::Instrument bass;
            bass.wave = mge::Waveform::Triangle;
            bass.envelope = {0.010f, 0.08f, 0.60f, 0.08f};
            bass.gain = 0.26f;

            const int iLead = song.addInstrument(lead);
            const int iBass = song.addInstrument(bass);
            const int patLead = song.pattern("C4 . E4 . | G4 - E4 . | A4 . G4 . | E4 - - . ");
            const int patBass = song.pattern("C3 . C3 . | E3 . E3 . | F3 . F3 . | G3 . G3 . ");
            song.addTrack(iLead, {patLead, patLead});
            song.addTrack(iBass, {patBass, patBass});

            m_themeSong = m_audio->addSong(song);
            m_audio->setBusVolume(mge::AudioBus::Music, 0.45f);
            m_audio->playSong(m_themeSong, 1.0f);

            reset();
            MGE_LOG_INFO("GuideGame",
                         "Juego iniciado. WASD: mover, Espacio: saltar, R: reiniciar, Esc: salir");
            return true;
        }

        void onFixedUpdate(float fixedDt) override {
            if (m_gameOver) return;

            // Simulación física de paso fijo (60 Hz determinista)
            const float moveX = m_platform->axis("MoveX");
            const float moveZ = m_platform->axis("MoveZ");

            // Aceleración horizontal y fricción
            const glm::vec3 targetHorizontalVel(moveX * 12.0f, 0.0f, moveZ * 12.0f);
            m_playerVel.x =
                glm::mix(m_playerVel.x, targetHorizontalVel.x, std::clamp(10.0f * fixedDt, 0.0f, 1.0f));
            m_playerVel.z =
                glm::mix(m_playerVel.z, targetHorizontalVel.z, std::clamp(10.0f * fixedDt, 0.0f, 1.0f));

            // Gravedad y salto
            m_playerVel.y -= 26.0f * fixedDt;
            const bool grounded = (m_playerPos.y <= 0.61f);
            if (grounded && m_platform->isActionDown("Jump")) {
                m_playerVel.y = 10.5f;
                m_audio->play(m_jumpSfx);
            }

            // Integración de posición
            m_playerPos += m_playerVel * fixedDt;

            // Colisión con el piso
            if (m_playerPos.y < 0.6f) {
                m_playerPos.y = 0.6f;
                m_playerVel.y = 0.0f;
            }

            // Límites de la arena (paredes)
            constexpr float kArenaBound = 16.5f;
            m_playerPos.x = glm::clamp(m_playerPos.x, -kArenaBound, kArenaBound);
            m_playerPos.z = glm::clamp(m_playerPos.z, -kArenaBound, kArenaBound);

            // Colección de gemas
            for (auto& gem : m_gems) {
                const float dist = glm::distance(m_playerPos, gem.position);
                if (dist < 1.1f) {
                    m_score += 100 * m_combo;
                    m_gemsCollected++;
                    m_combo++;
                    m_audio->play(m_coinSfx);

                    // Reubicar gema en una nueva posición aleatoria
                    gem.position.x = randomRange(-14.0f, 14.0f);
                    gem.position.z = randomRange(-14.0f, 14.0f);
                }
            }

            // Colisión con obstáculos peligrosos
            if (m_invulnerableTimer <= 0.0f) {
                for (const auto& hazard : m_hazards) {
                    const float dist = glm::distance(glm::vec2(m_playerPos.x, m_playerPos.z),
                                                     glm::vec2(hazard.position.x, hazard.position.z));
                    if (dist < hazard.radius + 0.6f && m_playerPos.y < 1.4f) {
                        m_health--;
                        m_combo = 1;
                        m_invulnerableTimer = 1.0f;
                        m_audio->play(m_hurtSfx);

                        // Impulso de rebote fuera del peligro
                        const glm::vec3 knockback = glm::normalize(m_playerPos - hazard.position);
                        m_playerVel.x = knockback.x * 12.0f;
                        m_playerVel.z = knockback.z * 12.0f;
                        m_playerVel.y = 7.0f;

                        if (m_health <= 0) {
                            m_gameOver = true;
                        }
                        break;
                    }
                }
            }
        }

        void onUpdate(float dt) override {
            if (m_platform->wasActionPressed("Quit")) {
                requestStop();
                return;
            }

            if (m_gameOver && m_platform->wasActionPressed("Restart")) {
                reset();
            }

            if (m_invulnerableTimer > 0.0f) {
                m_invulnerableTimer = std::max(0.0f, m_invulnerableTimer - dt);
            }

            // Animación visual de flotación de gemas
            for (auto& gem : m_gems) {
                gem.bobPhase += 3.5f * dt;
                gem.position.y = gem.baseHeight + std::sin(gem.bobPhase) * 0.25f;
            }
        }

        void onRender() override {
            const mge::GpuFrame* frame = m_gpu->frame();
            if (!frame || !m_renderer) return;

            // 1. Renderizado 3D de la escena con cámara y sombras aproximadas
            const float aspect =
                static_cast<float>(frame->width) / static_cast<float>(frame->height > 0 ? frame->height : 1);
            const glm::mat4 proj = glm::perspectiveRH_ZO(glm::radians(55.0f), aspect, 0.1f, 100.0f);

            const glm::vec3 cameraPos = m_playerPos + glm::vec3(0.0f, 10.0f, 13.0f);
            const glm::mat4 view = glm::lookAt(cameraPos, m_playerPos + glm::vec3(0.0f, 0.5f, 0.0f),
                                               glm::vec3(0.0f, 1.0f, 0.0f));

            if (m_renderer->begin(*frame->cmd, frame->swapchain, frame->width, frame->height,
                                  glm::vec4(0.10f, 0.12f, 0.16f, 1.0f))) {
                m_renderer->setCamera(proj * view, cameraPos);
                m_renderer->setLighting(m_lighting);

                // Suelo de la arena
                m_renderer->draw(m_floorMesh, glm::mat4(1.0f));

                // Columnas de borde
                constexpr float kBound = 17.5f;
                for (float coord = -kBound; coord <= kBound; coord += 3.5f) {
                    m_renderer->draw(m_wallMesh,
                                     glm::translate(glm::mat4(1.0f), glm::vec3(coord, 1.25f, -kBound)));
                    m_renderer->draw(m_wallMesh,
                                     glm::translate(glm::mat4(1.0f), glm::vec3(coord, 1.25f, kBound)));
                    m_renderer->draw(m_wallMesh,
                                     glm::translate(glm::mat4(1.0f), glm::vec3(-kBound, 1.25f, coord)));
                    m_renderer->draw(m_wallMesh,
                                     glm::translate(glm::mat4(1.0f), glm::vec3(kBound, 1.25f, coord)));
                }

                // Obstáculos peligrosos
                for (const auto& hazard : m_hazards) {
                    const glm::mat4 model = glm::translate(glm::mat4(1.0f), hazard.position);
                    m_renderer->draw(m_hazardMesh, model);
                }

                // Gemas giratorias
                for (const auto& gem : m_gems) {
                    glm::mat4 model = glm::translate(glm::mat4(1.0f), gem.position);
                    model = glm::rotate(model, gem.bobPhase * 1.5f, glm::vec3(0.0f, 1.0f, 0.0f));
                    m_renderer->draw(m_gemMesh, model);
                }

                // Orbe del jugador
                const glm::mat4 playerModel = glm::translate(glm::mat4(1.0f), m_playerPos);
                m_renderer->draw(m_playerMesh, playerModel);

                m_renderer->end();
            }

            // 2. HUD 2D inmediato en espacio de pantalla (Renderer2D)
            m_ui->begin(frame->width, frame->height, 1.0f);

            // Panel superior izquierdo: Puntaje y combo
            std::string scoreStr = "PUNTAJE: " + std::to_string(m_score);
            if (m_combo > 1) scoreStr += "  (x" + std::to_string(m_combo) + ")";
            m_ui->text({20.0f, 20.0f}, scoreStr, glm::vec4(1.0f), 2.0f);
            m_ui->text({20.0f, 48.0f}, "GEMAS: " + std::to_string(m_gemsCollected),
                       glm::vec4(0.9f, 0.9f, 0.4f, 1.0f), 2.0f);

            // Panel superior derecho: Salud
            std::string healthStr = "SALUD: ";
            for (int h = 0; h < m_health; ++h) healthStr += "[#] ";
            for (int h = m_health; h < 3; ++h) healthStr += "[ ] ";
            const glm::vec4 healthColor =
                (m_health > 1) ? glm::vec4(0.3f, 1.0f, 0.4f, 1.0f) : glm::vec4(1.0f, 0.3f, 0.3f, 1.0f);
            m_ui->text({float(frame->width) - 220.0f, 20.0f}, healthStr, healthColor, 2.0f);

            // Banner central de Game Over
            if (m_gameOver) {
                const float cx = float(frame->width) * 0.5f;
                const float cy = float(frame->height) * 0.5f;
                m_ui->rect({cx - 180.0f, cy - 60.0f}, {360.0f, 120.0f},
                           glm::vec4(0.06f, 0.08f, 0.12f, 0.92f));
                m_ui->rectOutline({cx - 180.0f, cy - 60.0f}, {360.0f, 120.0f},
                                  glm::vec4(1.0f, 0.3f, 0.3f, 1.0f));
                m_ui->text({cx - 85.0f, cy - 35.0f}, "FIN DEL JUEGO", glm::vec4(1.0f, 0.3f, 0.3f, 1.0f),
                           2.5f);
                m_ui->text({cx - 130.0f, cy + 15.0f}, "PULSA 'R' PARA REINICIAR", glm::vec4(1.0f), 1.8f);
            }

            m_ui->flush(*frame->cmd, frame->swapchain);
        }

        void onShutdown() override {
            // Liberar recursos de GPU antes de que el dispositivo GPU sea destruido
            m_floorMesh = {};
            m_wallMesh = {};
            m_playerMesh = {};
            m_gemMesh = {};
            m_hazardMesh = {};
            m_renderer.reset();
            m_ui.reset();

            MGE_LOG_INFO("GuideGame", "Juego cerrado limpiamente");
        }

    private:
        static mge::ApplicationConfig makeConfig() {
            mge::ApplicationConfig config;
            config.title = "miniGameEngine - Crystal Collector";
            config.width = 1280;
            config.height = 720;
            return config;
        }

        float randomRange(float minVal, float maxVal) {
            return minVal + m_rng.nextFloat01() * (maxVal - minVal);
        }

        void reset() {
            m_playerPos = glm::vec3(0.0f, 0.6f, 0.0f);
            m_playerVel = glm::vec3(0.0f);
            m_score = 0;
            m_combo = 1;
            m_gemsCollected = 0;
            m_health = 3;
            m_invulnerableTimer = 0.0f;
            m_gameOver = false;

            m_rng = mge::Pcg32(42);

            m_gems.clear();
            for (int i = 0; i < 6; ++i) {
                Gem gem;
                gem.position = glm::vec3(randomRange(-13.0f, 13.0f), 0.8f, randomRange(-13.0f, 13.0f));
                gem.bobPhase = randomRange(0.0f, 6.28f);
                m_gems.push_back(gem);
            }

            m_hazards.clear();
            for (int i = 0; i < 4; ++i) {
                Hazard h;
                h.position = glm::vec3(randomRange(-11.0f, 11.0f), 0.6f, randomRange(-11.0f, 11.0f));
                // Asegurar distancia mínima de inicio al jugador
                if (glm::length(h.position) < 3.0f) {
                    h.position.x += 5.0f;
                }
                m_hazards.push_back(h);
            }
        }

        mge::PlatformModule* m_platform = nullptr;
        mge::GpuModule* m_gpu = nullptr;
        mge::AudioModule* m_audio = nullptr;

        std::unique_ptr<mge::MeshRenderer> m_renderer;
        std::unique_ptr<mge::Renderer2D> m_ui;

        mge::GpuMesh m_floorMesh;
        mge::GpuMesh m_wallMesh;
        mge::GpuMesh m_playerMesh;
        mge::GpuMesh m_gemMesh;
        mge::GpuMesh m_hazardMesh;
        mge::Lighting m_lighting;

        mge::SampleId m_coinSfx = mge::kInvalidSample;
        mge::SampleId m_jumpSfx = mge::kInvalidSample;
        mge::SampleId m_hurtSfx = mge::kInvalidSample;
        mge::SongId m_themeSong = 0;

        glm::vec3 m_playerPos{0.0f, 0.6f, 0.0f};
        glm::vec3 m_playerVel{0.0f};
        int m_score = 0;
        int m_combo = 1;
        int m_gemsCollected = 0;
        int m_health = 3;
        float m_invulnerableTimer = 0.0f;
        bool m_gameOver = false;

        mge::Pcg32 m_rng{42};
        std::vector<Gem> m_gems;
        std::vector<Hazard> m_hazards;
    };

} // namespace

mge::Application* mge::createApplication() {
    return new GuideGame();
}
