#include <mge/mge.h>

#include <game/SandboxBlocks.h>
#include <game/SandboxMaterials.h>
#include <game/SandboxSounds.h>
#include <game/SandboxTerrain.h>
#include <SDL3/SDL.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace mge;

// Mallas en la GPU de un chunk (vacías si esa parte no tiene caras)
struct GpuChunk {
    GpuMesh opaque;
    GpuMesh cutout; // Hojas y plantas (prueba alfa)
    GpuMesh transparent;
    int lod = 0;
};

class SandboxApp : public mge::Application {
public:
    SandboxApp()
        : mge::Application(makeConfig()), m_blocks(sandbox::registerSandboxBlocks(m_registry)),
          m_world(m_registry, makeTerrain()) {
        // Módulos que usa el sandbox (orden de inicialización; se cierran al revés).
        // La ventana la toma la GPU de SDL (Vulkan) en el GpuModule
        addModule<PlatformModule>();
        addModule<GpuModule>();
        addModule<AudioModule>();
    }

    bool onInit() override {
        m_platform = getModule<PlatformModule>();
        // Eventos de ventana (tamaño, foco); el input del juego va por acciones
        m_platform->setEventCallback([this](const SDL_Event& event) { handleWindowEvent(event); });
        bindControls(m_platform->actions());

        // 1. Renderer (SDL3 GPU; los shaders vienen embebidos en el motor)
        m_gpu = getModule<GpuModule>();
        // Música ambiente generada por código (#49): suave, con eco; N la silencia
        m_audio = getModule<AudioModule>();
        m_audio->setDelay({0.45f, 0.4f, 0.35f});
        m_audio->setBusVolume(AudioBus::Music, kMusicVolume);
        m_daySongId = m_audio->addSong(songs::sandboxAmbient());
        m_nightSongId = m_audio->addSong(songs::sandboxNight());
        m_isNightMusic = (m_dayNight.hour >= 19.5f || m_dayNight.hour < 5.5f);
        m_audio->playSong(m_isNightMusic ? m_nightSongId : m_daySongId, 3.0f);
        m_soundIds = sandbox::registerSandboxSounds(*m_audio, m_sounds);
        m_renderer = std::make_unique<MeshRenderer>(m_gpu->device(), m_gpu->swapchainFormat());
        if (!m_renderer->valid()) return false;
        m_skyRenderer = std::make_unique<SkyRenderer>(m_gpu->device(), m_gpu->swapchainFormat(),
                                                      m_gpu->device().depthFormat());
        if (!m_skyRenderer->valid()) return false;
        m_debug = std::make_unique<DebugDraw>(m_gpu->device());
        // UI 2D (#42) y overlay de rendimiento (F1, #43): visible de entrada en Debug
        m_ui = std::make_unique<Renderer2D>(m_gpu->device(), m_gpu->swapchainFormat());
        if (!m_ui->valid()) return false;
#ifdef NDEBUG
        m_overlay.setVisible(false);
#endif
        // SANDBOX_DEBUG=1: arrancar con F1, F3 y F4 activos (lo usa un smoke test: dibuja todo en CI)
        if (const char* debug = std::getenv("SANDBOX_DEBUG"); debug && std::string(debug) == "1") {
            m_showDebug = true;
            m_frustumFrozen = true;
            m_overlay.setVisible(true);
        }
        if (const char* lodEnv = std::getenv("MGE_LOD"); lodEnv && std::string_view(lodEnv) == "0") {
            m_lodEnabled = false;
        }
        // Sol, ambiente cielo/suelo y niebla del color del cielo, que termina en el borde de lo que se dibuja
        updateFog();

        // 2. Jugador y cámara
        // Aparecer sobre el terreno (o sobre el agua) en el centro de la columna (0, 0)
        const int ground = std::max(m_terrain->surfaceHeight(0, 0), sandbox::SandboxTerrain::kWaterLevel);
        CharacterController playerController;
        const glm::vec3 spawnFeet(0.5f, static_cast<float>(ground) + 2.0f, 0.5f);
        playerController.previousPosition = spawnFeet;
        playerController.enabled = m_world.isLoaded(chunkOf(blockAt(spawnFeet))) &&
                                   m_world.isLoaded(chunkOf(blockAt(spawnFeet) - glm::ivec3(0, 1, 0)));
        m_prevFeet = spawnFeet;
        m_wasGrounded = playerController.grounded;

        m_playerEntity = world().registry().create();
        world().registry().add<Transform>(m_playerEntity, spawnFeet);
        world().registry().add<CharacterController>(m_playerEntity, playerController);

        m_cameraEntity = world().registry().create();
        world().registry().add<Transform>(m_cameraEntity);
        world().registry().add<Camera>(m_cameraEntity);
        world().registry().add<CharacterCamera>(m_cameraEntity, CharacterCamera{m_playerEntity});

        addCharacterSystem(world(), solidQuery(m_world));
        addCharacterCameraSystem(world(), time());
        m_physicsWorld.setVoxels(solidQuery(m_world));
        addPhysicsSystem(world(), m_physicsWorld);

        m_hotbarBlocks = {m_blocks.grass,       m_blocks.dirt,   m_blocks.stone,
                          m_blocks.cobblestone, m_blocks.planks, m_blocks.brick,
                          m_blocks.log,         m_blocks.glass,  m_blocks.leaves};

        // 3. Atlas: por defecto el procedural (#34, sin imágenes). El PNG es opt-in con SANDBOX_ATLAS=png
        // o con la tecla M en vivo. Si el PNG no existe, se registra el aviso una vez y se queda en
        // procedural.
        const char* atlasMode = std::getenv("SANDBOX_ATLAS");
        const bool usePng = atlasMode && std::string(atlasMode) == "png";
        if (!(usePng ? usePngAtlas() : useProceduralAtlas())) return false;

        // 4. Streaming del mundo en el pool de hilos del motor
        m_streamer = std::make_unique<ChunkStreamer>(m_world, streamingSettings(), &jobs());
        m_streamer->onChunkInserted([this](const ChunkCoord& coord, ChunkData& data) {
            const auto it = m_edits.find(coord);
            if (it != m_edits.end()) {
                for (const auto& [localIndex, blockId] : it->second) {
                    const int x = localIndex % kChunkSize;
                    const int z = (localIndex / kChunkSize) % kChunkSize;
                    const int y = localIndex / (kChunkSize * kChunkSize);
                    data.set(x, y, z, blockId);
                }
            }
        });

        // Configuración inicial del ratón
        m_platform->setCursorLocked(true);

        // F1 muestra FPS, tiempos y contadores en pantalla (fuente embebida, #43); además cada 5 s van al log
        MGE_LOG_INFO("Sandbox", "WASD: mover, Espacio: saltar, F: caminar/volar, Clic izq: romper, Clic der: "
                                "colocar, 1-9: hotbar, "
                                "F11: HUD, M: atlas PNG/procedural, N: música, T: avanzar hora, P: pausa, "
                                "F1: rendimiento, F2: LOD, "
                                "F3/F4: depuración, G: lanzar cubo, Esc: salir");
        return true;
    }

    void onShutdown() override {
        for (const auto& entry : profiler().entries()) {
            MGE_LOG_INFO("Profiler", entry.name, ": avg ", entry.averageMs, " ms, last ", entry.lastMs,
                         " ms, max ", entry.maxMs, " ms");
        }
        // Recursos de la GPU antes que el device (el GpuModule se cierra después del juego)
        m_thrownBodies.clear();
        m_streamer.reset(); // Los trabajos ya terminaron (run() los espera antes de onShutdown)
        m_gpuChunks.clear();
        m_pngAtlas = {};
        m_proceduralAtlas = {};
        m_atlas = nullptr;
        m_debug.reset();
        m_ui.reset();
        m_skyRenderer.reset();
        m_renderer.reset();
    }

    // Controles del sandbox (mismas teclas que antes): cambiar un control = cambiar un binding
    static void bindControls(mge::ActionMap& actions) {
        actions.bindAxis("MoveForward", Key::W, Key::S);
        actions.bindAxis("MoveRight", Key::D, Key::A);
        actions.bindAxis("MoveUp", Key::Space, Key::LeftShift);
        actions.bind("Jump", Key::Space);
        actions.bind("ToggleFly", Key::F);
        actions.bind("Sprint", Key::LeftCtrl);
        actions.bind("Throw", Key::G);
        actions.bind("Pause", Key::P);
        actions.bind("ToggleAtlas", Key::M);
        actions.bind("Overlay", Key::F1);
        actions.bind("ToggleLod", Key::F2);
        actions.bind("ToggleMusic", Key::N);
        actions.bind("DebugDraw", Key::F3);
        actions.bind("FreezeFrustum", Key::F4);
        actions.bind("ToggleHud", Key::F11);
        actions.bind("FastTime", Key::T);
        actions.bind("Quit", Key::Escape);
        actions.bind("Break", MouseButton::Left);
        actions.bind("Place", MouseButton::Right);
        actions.bind("CaptureMouse", MouseButton::Left);

        actions.bind("Slot1", Key::Num1);
        actions.bind("Slot2", Key::Num2);
        actions.bind("Slot3", Key::Num3);
        actions.bind("Slot4", Key::Num4);
        actions.bind("Slot5", Key::Num5);
        actions.bind("Slot6", Key::Num6);
        actions.bind("Slot7", Key::Num7);
        actions.bind("Slot8", Key::Num8);
        actions.bind("Slot9", Key::Num9);
    }

    // El cambio de tamaño ya no necesita código: el motor ajusta las cámaras al viewport (CameraSystem)
    void handleWindowEvent(const SDL_Event& event) {
        if (event.type == SDL_EVENT_WINDOW_FOCUS_GAINED && m_mouseCaptured && !m_isPaused) {
            m_platform->setCursorLocked(true);
        }
    }

    void handleActions() {
        if (m_platform->wasActionPressed("Quit")) requestStop();
        if (m_platform->wasActionPressed("Overlay")) m_overlay.toggle();
        if (m_platform->wasActionPressed("ToggleHud")) m_showHud = !m_showHud;

        static constexpr const char* kSlotActions[9] = {"Slot1", "Slot2", "Slot3", "Slot4", "Slot5",
                                                        "Slot6", "Slot7", "Slot8", "Slot9"};
        for (int i = 0; i < 9; ++i) {
            if (m_platform->wasActionPressed(kSlotActions[i])) {
                m_selectedSlot = i;
            }
        }
        if (m_platform->wasActionPressed("ToggleLod")) {
            m_lodEnabled = !m_lodEnabled;
            m_streamer->setSettings(streamingSettings());
            updateFog();
            if (!m_lodEnabled) {
                const glm::vec3 playerPos = world().registry().get<Transform>(m_playerEntity).position;
                const ChunkCoord center = chunkOf(blockAt(playerPos));
                for (auto it = m_gpuChunks.begin(); it != m_gpuChunks.end();) {
                    if (std::max(std::abs(it->first.x - center.x), std::abs(it->first.z - center.z)) >
                        kBaseRenderDistance) {
                        it = m_gpuChunks.erase(it);
                    } else {
                        ++it;
                    }
                }
            }
            MGE_LOG_INFO("Sandbox", m_lodEnabled ? "LOD: activado (doble distancia, 17x17 chunks)"
                                                 : "LOD: desactivado (distancia base, 9x9 chunks)");
        }
        if (m_platform->wasActionPressed("Throw")) spawnThrownBody();
        if (m_platform->wasActionPressed("ToggleFly")) {
            auto& controller = world().registry().get<CharacterController>(m_playerEntity);
            controller.toggleMode();
            MGE_LOG_INFO("Sandbox", controller.mode == CharacterMode::Fly ? "Modo de vuelo (Fly)"
                                                                          : "Modo caminata (Walk)");
        }
        if (m_platform->wasActionPressed("ToggleMusic")) {
            m_musicOn = !m_musicOn;
            m_audio->setBusVolume(AudioBus::Music, m_musicOn ? kMusicVolume : 0.0f);
        }
        if (m_platform->wasActionPressed("DebugDraw")) m_showDebug = !m_showDebug;
        if (m_platform->wasActionPressed("FreezeFrustum")) {
            m_frustumFrozen = !m_frustumFrozen;
            MGE_LOG_INFO("Sandbox", m_frustumFrozen
                                        ? "Frustum congelado (F4): volá afuera para ver el culling"
                                        : "Frustum libre");
        }
        if (m_platform->wasActionPressed("ToggleAtlas")) {
            if (m_atlas == &m_pngAtlas)
                useProceduralAtlas();
            else
                usePngAtlas();
        }

        // Romper bloque (clic izquierdo con cursor bloqueado) o capturar cursor (si no está bloqueado)
        if (m_platform->wasActionPressed("Break")) {
            if (!m_platform->isCursorLocked()) {
                if (!m_isPaused) {
                    m_platform->setCursorLocked(true);
                    m_breakCapturedThisClick = true;
                }
            } else if (!m_isPaused) {
                breakBlock();
                m_breakTimer = 0.25f;
            }
        } else if (m_platform->isActionDown("Break")) {
            if (!m_breakCapturedThisClick && m_platform->isCursorLocked() && !m_isPaused) {
                m_breakTimer -= time().delta();
                if (m_breakTimer <= 0.0f) {
                    breakBlock();
                    m_breakTimer += 0.25f;
                }
            }
        } else {
            m_breakTimer = 0.0f;
            m_breakCapturedThisClick = false;
        }

        // Colocar bloque (clic derecho) con repetición cada 0.25 s
        if (m_platform->wasActionPressed("Place")) {
            if (m_platform->isCursorLocked() && !m_isPaused) {
                placeBlock();
                m_placeTimer = 0.25f;
            }
        } else if (m_platform->isActionDown("Place")) {
            if (m_platform->isCursorLocked() && !m_isPaused) {
                m_placeTimer -= time().delta();
                if (m_placeTimer <= 0.0f) {
                    placeBlock();
                    m_placeTimer += 0.25f;
                }
            }
        } else {
            m_placeTimer = 0.0f;
        }

        if (m_platform->wasActionPressed("Pause")) {
            m_isPaused = !m_isPaused;
            time().setTimeScale(m_isPaused ? 0.0f : 1.0f);
            m_platform->setCursorLocked(!m_isPaused);
        }
    }

    void spawnThrownBody() {
        const auto& controller = world().registry().get<CharacterController>(m_playerEntity);
        const auto& transform = world().registry().get<Transform>(m_playerEntity);
        const glm::vec3 eyePos = transform.position + glm::vec3(0.0f, controller.eyeHeight, 0.0f);
        const glm::vec3 halfExtents(0.25f); // Tamaño total 0.5
        const glm::vec3 fwd = controller.forward();

        auto overlapsSolid = [this](const Aabb& aabb) {
            constexpr float kTiny = 1e-4f;
            const int minX = static_cast<int>(std::floor(aabb.min.x + kTiny));
            const int maxX = static_cast<int>(std::floor(aabb.max.x - kTiny));
            const int minY = static_cast<int>(std::floor(aabb.min.y + kTiny));
            const int maxY = static_cast<int>(std::floor(aabb.max.y - kTiny));
            const int minZ = static_cast<int>(std::floor(aabb.min.z + kTiny));
            const int maxZ = static_cast<int>(std::floor(aabb.max.z - kTiny));
            for (int y = minY; y <= maxY; ++y) {
                for (int z = minZ; z <= maxZ; ++z) {
                    for (int x = minX; x <= maxX; ++x) {
                        if (m_world.isSolid(glm::ivec3(x, y, z))) {
                            return true;
                        }
                    }
                }
            }
            return false;
        };

        glm::vec3 spawnPos = eyePos;
        const Aabb initialBounds(spawnPos - halfExtents, spawnPos + halfExtents);
        if (overlapsSolid(initialBounds)) {
            constexpr float kMaxBacktrack = 0.5f;
            constexpr int kSteps = 10;
            bool foundFree = false;
            for (int i = 1; i <= kSteps; ++i) {
                const float backDist = (kMaxBacktrack / static_cast<float>(kSteps)) * static_cast<float>(i);
                const glm::vec3 candidatePos = eyePos - fwd * backDist;
                const Aabb candidateBounds(candidatePos - halfExtents, candidatePos + halfExtents);
                if (!overlapsSolid(candidateBounds)) {
                    spawnPos = candidatePos;
                    foundFree = true;
                    break;
                }
            }
            if (!foundFree) {
                return; // Omitir el lanzamiento si el cuerpo nace superpuesto y no hay espacio retrocediendo
            }
        }

        if (m_thrownBodies.size() >= 64) {
            const Entity oldest = m_thrownBodies.front();
            m_thrownBodies.pop_front();
            world().registry().destroy(oldest);
        }

        const Entity bodyEntity = world().registry().create();
        world().registry().add<Transform>(bodyEntity, spawnPos);
        RigidBody rb;
        rb.shape = ShapeType::Box;
        rb.halfExtents = halfExtents;
        rb.mass = 1.0f;
        rb.restitution = 0.4f;
        rb.friction = 0.5f;
        rb.velocity = fwd * 15.0f;
        rb.previousPosition = spawnPos;
        world().registry().add<RigidBody>(bodyEntity, rb);
        m_thrownBodies.push_back(bodyEntity);

        const float pan = (m_audioRng.nextFloat01() * 2.0f - 1.0f) * 0.15f;
        m_audio->play(m_soundIds.throwSound, 0.4f, pan);
    }

    void recordEdit(const glm::ivec3& block, BlockId blockId) {
        const ChunkCoord coord = chunkOf(block);
        const glm::ivec3 local = localOf(block);
        const auto localIdx = static_cast<std::uint16_t>(ChunkData::index(local.x, local.y, local.z));
        auto& edits = m_edits[coord];
        for (auto& entry : edits) {
            if (entry.first == localIdx) {
                entry.second = blockId;
                return;
            }
        }
        edits.emplace_back(localIdx, blockId);
    }

    static bool overlapsHalfOpen(const Aabb& a, const Aabb& b) {
        constexpr float kEps = 1e-4f;
        return a.min.x < b.max.x - kEps && a.max.x > b.min.x + kEps && a.min.y < b.max.y - kEps &&
               a.max.y > b.min.y + kEps && a.min.z < b.max.z - kEps && a.max.z > b.min.z + kEps;
    }

    void updateSelection() {
        const auto* controller = world().registry().tryGet<CharacterController>(m_playerEntity);
        const auto* playerTransform = world().registry().tryGet<Transform>(m_playerEntity);
        if (!controller || !playerTransform) {
            m_selectionHit = std::nullopt;
            return;
        }
        const glm::vec3 eye = characterEyePosition(*controller, *playerTransform, time().alpha());
        const glm::vec3 forward = controller->forward();
        const Ray ray(eye, forward);

        auto pickable = [this](BlockId id) {
            return id != kAir && id != m_blocks.water && id != m_blocks.lava;
        };
        constexpr float kReachDistance = 6.0f;
        m_selectionHit = raycastVoxels(m_world, ray, kReachDistance, pickable);
    }

    void breakBlock() {
        if (!m_selectionHit) return;
        const glm::ivec3 block = m_selectionHit->block;
        const BlockId brokenId = m_world.getBlock(block);
        if (brokenId != kAir) {
            const auto mat = static_cast<std::size_t>(sandbox::soundMaterialForBlock(brokenId, m_registry));
            const std::size_t variant = m_audioRng.range(0, sandbox::SandboxSounds::kBreakVariants);
            const float pan = (m_audioRng.nextFloat01() * 2.0f - 1.0f) * 0.15f;
            m_audio->play(m_soundIds.materials[mat].breakVariants[variant], 0.45f, pan);
        }
        m_world.setBlock(block, kAir);
        recordEdit(block, kAir);
        updateSelection();
    }

    void placeBlock() {
        if (!m_selectionHit) return;
        const BlockId toPlace = m_hotbarBlocks[static_cast<std::size_t>(m_selectedSlot)];
        if (toPlace == kAir) return;

        const glm::ivec3 placePos = m_selectionHit->block + m_selectionHit->normal;
        const BlockId currentId = m_world.getBlock(placePos);
        if (currentId != kAir && currentId != m_blocks.water) return;

        const Aabb blockBox(glm::vec3(placePos), glm::vec3(placePos) + glm::vec3(1.0f));

        const auto* playerTf = world().registry().tryGet<Transform>(m_playerEntity);
        const auto* controller = world().registry().tryGet<CharacterController>(m_playerEntity);
        if (playerTf && controller) {
            const Aabb playerBox = characterAabb(playerTf->position, controller->halfExtents);
            if (overlapsHalfOpen(blockBox, playerBox)) return;
        }

        for (const Entity e : m_thrownBodies) {
            if (!world().registry().valid(e)) continue;
            const auto* rb = world().registry().tryGet<RigidBody>(e);
            const auto* tf = world().registry().tryGet<Transform>(e);
            if (rb && tf) {
                if (overlapsHalfOpen(blockBox, rb->bounds(tf->position))) return;
            }
        }

        if (m_world.setBlock(placePos, toPlace)) {
            recordEdit(placePos, toPlace);
            const auto mat = static_cast<std::size_t>(sandbox::soundMaterialForBlock(toPlace, m_registry));
            const std::size_t variant = m_audioRng.range(0, sandbox::SandboxSounds::kPlaceVariants);
            const float pan = (m_audioRng.nextFloat01() * 2.0f - 1.0f) * 0.15f;
            m_audio->play(m_soundIds.materials[mat].placeVariants[variant], 0.4f, pan);
            updateSelection();
        }
    }

    void onUpdate(float /*dt*/) override {
        updateSelection();
        handleActions();

        // Control del personaje: mirar con el ratón cuando el cursor está capturado y no en pausa
        auto& controller = world().registry().get<CharacterController>(m_playerEntity);
        if (m_platform->isCursorLocked() && !m_isPaused) {
            constexpr float kSensitivity = 0.1f;
            controller.look(m_platform->input().mouseDelta().x * kSensitivity,
                            -m_platform->input().mouseDelta().y * kSensitivity);
        }

        // Intención de movimiento
        controller.input.moveForward = m_platform->axis("MoveForward");
        controller.input.moveRight = m_platform->axis("MoveRight");
        controller.input.moveUp = m_platform->axis("MoveUp");
        controller.input.sprint = m_platform->isActionDown("Sprint");
        controller.input.jump = m_platform->isActionDown("Jump");

        // Salto: sonido al impulsarse en modo caminata estando apoyado
        if (m_platform->wasActionPressed("Jump") && controller.mode == CharacterMode::Walk &&
            controller.grounded && !m_isPaused) {
            const float pan = (m_audioRng.nextFloat01() * 2.0f - 1.0f) * 0.1f;
            m_audio->play(m_soundIds.jump, 0.35f, pan);
        }

        // Aterrizaje: transición false -> true con caída rápida (|velocity.y| > 6)
        if (!controller.grounded) {
            if (controller.velocity.y < 0.0f) {
                m_lastFallSpeed = std::max(m_lastFallSpeed, -controller.velocity.y);
            }
        } else {
            if (!m_wasGrounded && m_lastFallSpeed > 6.0f && !m_isPaused) {
                const float gain = std::clamp((m_lastFallSpeed - 6.0f) / 12.0f + 0.3f, 0.3f, 1.0f);
                const float pan = (m_audioRng.nextFloat01() * 2.0f - 1.0f) * 0.1f;
                m_audio->play(m_soundIds.land, gain, pan);
            }
            m_lastFallSpeed = 0.0f;
        }
        m_wasGrounded = controller.grounded;

        // Seguridad de streaming: no simular hasta que el chunk bajo los pies y el inferior estén cargados
        const glm::vec3 feet = world().registry().get<Transform>(m_playerEntity).position;
        controller.enabled = m_world.isLoaded(chunkOf(blockAt(feet))) &&
                             m_world.isLoaded(chunkOf(blockAt(feet) - glm::ivec3(0, 1, 0)));

        // Pasos: cada 0.45 bloques caminando (0.3 al correr) sobre el bloque bajo los pies
        if (controller.mode == CharacterMode::Walk && controller.grounded && !m_isPaused) {
            const float dx = feet.x - m_prevFeet.x;
            const float dz = feet.z - m_prevFeet.z;
            const float dist = std::sqrt(dx * dx + dz * dz);
            if (dist < 2.0f) {
                m_stepAccumulator += dist;
            }
            const float stepInterval = controller.input.sprint ? 0.3f : 0.45f;
            if (m_stepAccumulator >= stepInterval) {
                m_stepAccumulator = std::fmod(m_stepAccumulator, stepInterval);

                glm::ivec3 blockPos = blockAt(feet - glm::vec3(0.0f, 0.1f, 0.0f));
                BlockId blockId = m_world.getBlock(blockPos);
                if (blockId == kAir) {
                    blockPos = blockAt(feet - glm::vec3(0.0f, 0.5f, 0.0f));
                    blockId = m_world.getBlock(blockPos);
                }
                const auto mat =
                    static_cast<std::size_t>(sandbox::soundMaterialForBlock(blockId, m_registry));
                const std::size_t variant = m_audioRng.range(0, sandbox::SandboxSounds::kFootstepVariants);
                const float pan = (m_audioRng.nextFloat01() * 2.0f - 1.0f) * 0.15f;
                const float gain = controller.input.sprint ? 0.35f : 0.25f;
                m_audio->play(m_soundIds.materials[mat].footsteps[variant], gain, pan);
            }
        } else {
            m_stepAccumulator = 0.0f;
        }
        m_prevFeet = feet;

        // Chapuzón al ingresar en agua
        const BlockId feetBlock = m_world.getBlock(blockAt(feet + glm::vec3(0.0f, 0.2f, 0.0f)));
        const bool inWater = (feetBlock == m_blocks.water);
        if (inWater && !m_wasInWater && !m_isPaused) {
            const float pan = (m_audioRng.nextFloat01() * 2.0f - 1.0f) * 0.1f;
            m_audio->play(m_soundIds.splash, 0.5f, pan);
        }
        m_wasInWater = inWater;

        // Animaciones
        m_animTimer += time().unscaledDelta();
        if (m_animTimer >= 0.2f) {
            m_animTimer = 0.0f;
            m_currentAnimFrame++;
            copyTile(sandbox::kWaterFrames[m_currentAnimFrame % sandbox::kWaterFrames.size()],
                     sandbox::kWaterTile);
            copyTile(sandbox::kLavaFrames[m_currentAnimFrame % sandbox::kLavaFrames.size()],
                     sandbox::kLavaTile);
        }

        // Ciclo día/noche (#35): T avanza el tiempo rápido (x60) mientras se mantenga presionada
        const float timeSpeed = m_platform->isActionDown("FastTime") ? 60.0f : 1.0f;
        const float timeDelta =
            m_isPaused ? (m_platform->isActionDown("FastTime") ? time().unscaledDelta() * 60.0f : 0.0f)
                       : (time().delta() * timeSpeed);
        m_dayNight.advance(timeDelta);
        updateFog();

        // Música día / noche con fundido cruzado de 4 s (#64)
        const bool isNight = (m_dayNight.hour >= 19.5f || m_dayNight.hour < 5.5f);
        if (isNight != m_isNightMusic) {
            m_isNightMusic = isNight;
            m_audio->playSong(m_isNightMusic ? m_nightSongId : m_daySongId, 4.0f);
        }

        streamChunks();
    }

    void onRender() override {
        const GpuFrame* frame = m_gpu->frame();
        if (!frame) return; // Ventana minimizada
        const glm::vec3 horizon = m_dayNight.sky().horizonColor;
        if (!m_renderer->begin(*frame->cmd, frame->swapchain, frame->width, frame->height,
                               glm::vec4(horizon, 1.0f)))
            return;
        // La cámara del ECS: el aspecto ya está ajustado al tamaño real del swapchain (CameraSystem)
        Registry& registry = world().registry();
        const CameraMatrices camera = cameraMatrices(registry.get<Transform>(m_cameraEntity).localMatrix(),
                                                     registry.get<Camera>(m_cameraEntity), viewport());
        m_renderer->setCamera(camera.viewProjection, camera.position);
        if (m_skyRenderer) {
            m_skyRenderer->draw(*m_renderer, glm::inverse(camera.viewProjection), camera.position,
                                m_dayNight.sky());
        }
        // F4: el culling usa el frustum congelado (se ve qué se descarta al salir de él)
        if (!m_frustumFrozen) m_frozenViewProjection = camera.viewProjection;
        const Frustum cullFrustum =
            m_frustumFrozen ? Frustum::fromViewProjection(m_frozenViewProjection) : camera.frustum;

        // Cola de dibujo: opaco y cutout de adelante hacia atrás, transparente de atrás hacia adelante
        m_queue.clear();
        for (auto& [coord, gpu] : m_gpuChunks) {
            const Aabb bounds = chunkBounds(coord);
            if (!cullFrustum.intersects(bounds)) continue;
            const glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(chunkOrigin(coord)));
            m_queue.add({&gpu.opaque, model, m_atlas, MeshPass::Opaque, bounds.center(), {}});
            m_queue.add({&gpu.cutout, model, m_atlas, MeshPass::Cutout, bounds.center(), {}});
            m_queue.add({&gpu.transparent, model, m_atlas, MeshPass::Transparent, bounds.center(), {}});
        }
        {
            MGE_PROFILE_SCOPE(profiler(), "queue sort/submit");
            const auto& playerController = world().registry().get<CharacterController>(m_playerEntity);
            m_queue.sort(camera.position, playerController.forward());
            m_renderer->submit(m_queue);
        }
        drawDebug();
        m_renderer->end();
        drawOverlay(*frame);

        // FPS promedio cada 5 s (tiempo real: en pausa el escalado es 0)
        m_fpsFrames++;
        m_fpsElapsed += time().unscaledDelta();
        if (m_fpsElapsed >= 5.0f) {
            [[maybe_unused]] const RenderStats& stats = m_renderer->stats();
            MGE_LOG_DEBUG("Sandbox", "FPS: ", static_cast<int>(m_fpsFrames / m_fpsElapsed), " (",
                          m_gpuChunks.size(), " chunks; dibujo: ", stats.totalDrawCalls(), " llamadas, ",
                          stats.totalTriangles(), " triángulos = opaco ", stats.triangles[0], " + cutout ",
                          stats.triangles[1], " + transparente ", stats.triangles[2], ")");
            [[maybe_unused]] const StreamingStats streaming = m_streamer->stats();
            MGE_LOG_DEBUG("Sandbox", "Streaming: ", streaming.loaded, " cargados (",
                          streaming.cpuBytes() / 1024 / 1024, " MB CPU), ", m_gpuChunks.size(), " mallados (",
                          gpuBytes() / 1024 / 1024, " MB GPU), ", streaming.pendingGenerate, " + ",
                          streaming.pendingMesh, " trabajos, pool ", streaming.pooled, ", descartados ",
                          streaming.discarded);
            m_fpsFrames = 0;
            m_fpsElapsed = 0.0f;
        }
    }

private:
    static constexpr int kBaseRenderDistance = 4;   // Distancia base sin LOD (9×9 chunks)
    static constexpr int kDoubleRenderDistance = 8; // Distancia doblada con LOD (17×17 chunks)

    void updateFog() {
        const int renderDist = m_lodEnabled ? kDoubleRenderDistance : kBaseRenderDistance;
        Lighting base;
        base.fog = true;
        base.fogStart = (renderDist - 1.5f) * kChunkSize;
        base.fogEnd = renderDist * kChunkSize;
        const Lighting lighting = m_dayNight.lighting(base);
        if (m_renderer) m_renderer->setLighting(lighting);
    }

    // Caja del chunk en el mundo
    static mge::Aabb chunkBounds(const ChunkCoord& coord) {
        const glm::vec3 origin(chunkOrigin(coord));
        return mge::Aabb(origin, origin + glm::vec3(static_cast<float>(kChunkSize)));
    }

    // Streaming (#56, #57, #61): el ChunkStreamer genera y malla en el pool de hilos, de lo más cercano a lo
    // más lejano, y descarga lo que queda lejos (con histéresis). Acá solo se suben las mallas listas a la
    // GPU (como mucho maxResultsPerUpdate por frame) y se liberan las de los chunks descargados.
    void streamChunks() {
        MGE_PROFILE_SCOPE(profiler(), "streaming");
        const glm::vec3 playerPos = world().registry().get<Transform>(m_playerEntity).position;
        const ChunkCoord center = chunkOf(blockAt(playerPos));
        StreamUpdate update = m_streamer->update(center);
        {
            MGE_PROFILE_SCOPE(profiler(), "mesh upload");
            for (auto& [coord, mesh, lod] : update.meshes) {
                GpuChunk& gpu = m_gpuChunks[coord];
                gpu.opaque = m_gpu->device().uploadMesh(mesh.opaque);
                gpu.cutout = m_gpu->device().uploadMesh(mesh.cutout);
                gpu.transparent = m_gpu->device().uploadMesh(mesh.transparent);
                gpu.lod = lod;
            }
        }
        for (const ChunkCoord& coord : update.unloaded) m_gpuChunks.erase(coord);
    }

    StreamingSettings streamingSettings() const {
        StreamingSettings settings;
        if (m_lodEnabled) {
            settings.lodRadius = {kBaseRenderDistance, kDoubleRenderDistance, 0};
            settings.meshRadius = kDoubleRenderDistance;
            settings.loadRadius = kDoubleRenderDistance + 1;
        } else {
            settings.lodRadius = {0, 0, 0};
            settings.meshRadius = kBaseRenderDistance;
            settings.loadRadius = kBaseRenderDistance + 1;
        }
        settings.unloadMargin = 2;
        settings.minChunkY = 0;
        settings.maxChunkY = sandbox::SandboxTerrain::kWorldChunksY - 1;
        settings.maxJobsInFlight = 64;
        settings.maxResultsPerUpdate = 12; // Subidas a la GPU por frame: acota el trabajo del hilo principal
        settings.meshOptions.atlas = {sandbox::kAtlasColumns, sandbox::kAtlasRows};
        settings.meshOptions.ambientOcclusion = true; // Rincones más oscuros (color de vértice)
        settings.meshOptions.lodSkirts = true;
        return settings;
    }

    // Bytes aproximados de las mallas en la GPU (vértices de 48 B + índices de 4 B)
    std::size_t gpuBytes() const {
        std::size_t bytes = 0;
        for (const auto& [coord, gpu] : m_gpuChunks)
            for (const GpuMesh* mesh : {&gpu.opaque, &gpu.cutout, &gpu.transparent})
                bytes += mesh->vertexCount * sizeof(GpuVertex) + mesh->indexCount * sizeof(std::uint32_t);
        return bytes;
    }

    // El mundo es dueño del generador; el sandbox guarda un puntero para consultar alturas (spawn)
    std::unique_ptr<sandbox::SandboxTerrain> makeTerrain() {
        auto terrain = std::make_unique<sandbox::SandboxTerrain>(m_blocks);
        m_terrain = terrain.get();
        return terrain;
    }

    // HUD del juego (F11): punto de mira (crosshair), barra de acceso rápido (hotbar de 9 slots con el
    // tile superior de cada bloque) e indicador de modo ("Caminar" / "Volar") y la hora
    void drawHud(Renderer2D& ui) {
        if (!m_showHud) return;
        const glm::vec2 size = ui.size();

        // 1. Punto de mira (crosshair): dos rectángulos centrados en pantalla
        const glm::vec2 center = glm::floor(size * 0.5f);
        constexpr float kCrosshairLen = 10.0f;
        constexpr float kCrosshairThick = 2.0f;
        const glm::vec4 crosshairColor(1.0f, 1.0f, 1.0f, 0.8f);
        ui.rect({center.x - kCrosshairLen * 0.5f, center.y - kCrosshairThick * 0.5f},
                {kCrosshairLen, kCrosshairThick}, crosshairColor);
        ui.rect({center.x - kCrosshairThick * 0.5f, center.y - kCrosshairLen * 0.5f},
                {kCrosshairThick, kCrosshairLen}, crosshairColor);

        // 2. Hotbar de 9 slots abajo con la cara superior de cada bloque
        constexpr float kSlotSize = 28.0f;
        constexpr float kSlotMargin = 4.0f;
        constexpr float kIconPadding = 4.0f;
        constexpr float kIconSize = kSlotSize - 2.0f * kIconPadding;
        constexpr int kSlots = 9;
        const float totalWidth = kSlots * kSlotSize + (kSlots - 1) * kSlotMargin;
        const float startX = glm::floor((size.x - totalWidth) * 0.5f);
        const float startY = size.y - kSlotSize - 8.0f;

        const TextureAtlas atlas{sandbox::kAtlasColumns, sandbox::kAtlasRows};
        for (int i = 0; i < kSlots; ++i) {
            const glm::vec2 slotPos(startX + static_cast<float>(i) * (kSlotSize + kSlotMargin), startY);
            // Fondo del slot
            ui.rect(slotPos, {kSlotSize, kSlotSize}, {0.0f, 0.0f, 0.0f, 0.5f});
            // Borde: seleccionado destacado (2 px, blanco) o normal (1 px)
            if (i == m_selectedSlot) {
                ui.rectOutline(slotPos, {kSlotSize, kSlotSize}, {1.0f, 1.0f, 1.0f, 0.95f}, 2.0f);
            } else {
                ui.rectOutline(slotPos, {kSlotSize, kSlotSize}, {1.0f, 1.0f, 1.0f, 0.3f}, 1.0f);
            }

            // Cara superior del bloque desde el atlas
            const BlockId blockId = m_hotbarBlocks[static_cast<std::size_t>(i)];
            const BlockDesc& desc = m_registry.get(blockId);
            const int tile = desc.tile(BlockFace::Top);
            if (tile >= 0 && m_atlas) {
                const UvRect uv = atlas.tileUv(tile);
                ui.image(m_atlas, slotPos + glm::vec2(kIconPadding), {kIconSize, kIconSize}, uv.min, uv.max,
                         glm::vec4(1.0f), true);
            }
        }

        // 3. Indicador de modo ("Caminar" / "Volar") y la hora
        const auto* controller = world().registry().tryGet<CharacterController>(m_playerEntity);
        const char* modeStr = (controller && controller->mode == CharacterMode::Fly) ? "Volar" : "Caminar";
        const int hourInt = static_cast<int>(m_dayNight.hour);
        const int minuteInt = static_cast<int>((m_dayNight.hour - static_cast<float>(hourInt)) * 60.0f);
        char hourBuf[32];
        std::snprintf(hourBuf, sizeof(hourBuf), "%s  %02d:%02d", modeStr, hourInt, minuteInt);

        const glm::vec2 infoPos(8.0f, 8.0f);
        const float textWidth = static_cast<float>(std::strlen(hourBuf)) * 8.0f;
        ui.rect(infoPos, {textWidth + 8.0f, 16.0f}, {0.0f, 0.0f, 0.0f, 0.4f});
        ui.text(infoPos + glm::vec2(4.0f, 4.0f), hourBuf, {1.0f, 1.0f, 1.0f, 0.9f});
    }

    // F1: FPS, gráfico de tiempos y contadores, encima de la escena (Renderer2D)
    void drawOverlay(const GpuFrame& frame) {
        m_overlay.addFrame(time().unscaledDelta() * 1000.0f, frame.acquireMs);
        if (m_overlay.visible()) {
            const RenderStats& render = m_renderer->stats();
            const StreamingStats streaming = m_streamer->stats();
            const glm::vec3 playerPos = world().registry().get<Transform>(m_playerEntity).position;
            const glm::ivec3 block = blockAt(playerPos);
            const int hourInt = static_cast<int>(m_dayNight.hour);
            const int minuteInt = static_cast<int>((m_dayNight.hour - static_cast<float>(hourInt)) * 60.0f);
            char hourBuf[32];
            std::snprintf(hourBuf, sizeof(hourBuf), "%02d:%02d", hourInt, minuteInt);
            m_overlay.setCounter("hora", hourBuf);
            m_overlay.setCounter("dibujo", std::to_string(render.totalDrawCalls()) + " llamadas, " +
                                               std::to_string(render.totalTriangles() / 1000) +
                                               "k triangulos");
            std::array<int, 3> lodChunks = {0, 0, 0};
            std::array<std::size_t, 3> lodTris = {0, 0, 0};
            for (const auto& [coord, gpu] : m_gpuChunks) {
                const int l = std::clamp(gpu.lod, 0, 2);
                lodChunks[l]++;
                lodTris[l] +=
                    (gpu.opaque.indexCount + gpu.cutout.indexCount + gpu.transparent.indexCount) / 3;
            }
            m_overlay.setCounter(
                "lod", (m_lodEnabled ? "ON" : "OFF") + std::string(" (L0: ") + std::to_string(lodChunks[0]) +
                           "c/" + std::to_string(lodTris[0] / 1000) +
                           "k, L1: " + std::to_string(lodChunks[1]) + "c/" +
                           std::to_string(lodTris[1] / 1000) + "k, L2: " + std::to_string(lodChunks[2]) +
                           "c/" + std::to_string(lodTris[2] / 1000) + "k)");
            m_overlay.setCounter("chunks", std::to_string(streaming.loaded) + " cargados, " +
                                               std::to_string(m_gpuChunks.size()) + " mallados, pool " +
                                               std::to_string(streaming.pooled));
            m_overlay.setCounter("trabajos", std::to_string(streaming.pendingGenerate) + " generar + " +
                                                 std::to_string(streaming.pendingMesh) + " mallar");
            m_overlay.setCounter("memoria", std::to_string(streaming.cpuBytes() / 1024 / 1024) + " MB CPU, " +
                                                std::to_string(gpuBytes() / 1024 / 1024) + " MB GPU");
            m_overlay.setCounter("posicion", std::to_string(block.x) + " " + std::to_string(block.y) + " " +
                                                 std::to_string(block.z));
            if (m_selectionHit) {
                const BlockDesc& desc = m_registry.get(m_selectionHit->blockId);
                m_overlay.setCounter("seleccion", std::to_string(m_selectionHit->block.x) + " " +
                                                      std::to_string(m_selectionHit->block.y) + " " +
                                                      std::to_string(m_selectionHit->block.z) + " (" +
                                                      desc.name + ")");
            } else {
                m_overlay.setCounter("seleccion", "-");
            }
            m_overlay.setCounter("atlas", m_atlas == &m_pngAtlas ? "PNG" : "procedural");
            m_overlay.setCounter("cuerpos", std::to_string(m_thrownBodies.size()) + " (" +
                                                std::to_string(m_physicsWorld.lastPairCount()) + " pares)");
            m_overlay.setCounter(
                "", "Clic romper/colocar  1-9 hotbar  F1 panel  F2 LOD  F3 chunks  F4 frustum  F11 HUD  "
                    "F volar  G lanzar  M atlas  N musica  P pausa");

            m_overlay.setProfile(profiler());
        }

        // UI 2D: HUD debajo, F1 overlay encima (ambos con escala entera para píxeles nítidos)
        if (m_showHud || m_overlay.visible()) {
            MGE_PROFILE_SCOPE(profiler(), "UI");
            m_ui->begin(frame.width, frame.height, std::max(1.0f, std::round(frame.displayScale)));
            if (m_showHud) {
                drawHud(*m_ui);
            }
            if (m_overlay.visible()) {
                m_overlay.draw(*m_ui);
            }
            m_ui->flush(*frame.cmd, frame.swapchain);
        }
    }

    // F3: bordes de los chunks (verde = mallado, amarillo = con datos sin malla, rojo = sucio), la caja de
    // colisión del personaje y, con F4, el frustum congelado. El contorno del bloque seleccionado se dibuja
    // siempre que haya uno apuntado.
    void drawDebug() {
        m_debug->clear();

        // Contorno de selección del bloque (siempre visible, inflado por 0.002 en negro)
        if (m_selectionHit) {
            constexpr float kInflate = 0.002f;
            const glm::vec3 bMin = glm::vec3(m_selectionHit->block) - glm::vec3(kInflate);
            const glm::vec3 bMax = glm::vec3(m_selectionHit->block) + glm::vec3(1.0f + kInflate);
            m_debug->aabb(Aabb(bMin, bMax), glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
        }

        if (m_showDebug) {
            const auto* playerTransform = world().registry().tryGet<Transform>(m_playerEntity);
            const auto* controller = world().registry().tryGet<CharacterController>(m_playerEntity);
            const glm::vec3 feet = playerTransform ? playerTransform->position : glm::vec3(0.0f);
            const ChunkCoord center = chunkOf(blockAt(feet));
            const int renderDist = m_lodEnabled ? kDoubleRenderDistance : kBaseRenderDistance;
            for (const ChunkCoord& coord : m_world.loadedChunks()) {
                if (std::abs(coord.x - center.x) > renderDist || std::abs(coord.z - center.z) > renderDist)
                    continue;
                const glm::vec4 color = m_world.isDirty(coord)     ? glm::vec4(1.0f, 0.25f, 0.2f, 0.9f)
                                        : m_gpuChunks.count(coord) ? glm::vec4(0.3f, 1.0f, 0.4f, 0.5f)
                                                                   : glm::vec4(1.0f, 0.9f, 0.2f, 0.7f);
                m_debug->aabb(chunkBounds(coord), color);
            }
            if (controller) {
                m_debug->aabb(characterAabb(feet, controller->halfExtents), {1.0f, 1.0f, 1.0f, 1.0f});
            }
        }
        if (m_frustumFrozen) m_debug->frustum(m_frozenViewProjection, {1.0f, 0.5f, 0.1f, 1.0f});

        for (const Entity e : m_thrownBodies) {
            if (!world().registry().valid(e)) continue;
            const auto* rb = world().registry().tryGet<RigidBody>(e);
            const auto* tf = world().registry().tryGet<Transform>(e);
            if (rb && tf) {
                const glm::vec3 renderPos = glm::mix(rb->previousPosition, tf->position, time().alpha());
                m_debug->aabb(rb->bounds(renderPos), glm::vec4(1.0f, 0.6f, 0.1f, 1.0f));
            }
        }

        m_debug->flush(*m_renderer);
    }

    bool usePngAtlas() {
        if (!m_pngAtlas) {
            m_pngAtlas = Texture2D::fromFile(m_gpu->device(), "textures/Minecraft-like_3d_tileset.png", true);
            if (!m_pngAtlas) {
                if (!m_pngMissingLogged) {
                    MGE_LOG_WARN("Sandbox", "No se encontró el atlas PNG; permaneciendo en atlas procedural");
                    m_pngMissingLogged = true;
                }
                return useProceduralAtlas();
            }
        }
        m_atlas = &m_pngAtlas;
        MGE_LOG_INFO("Sandbox", "Atlas: PNG (assets)");
        return true;
    }

    bool useProceduralAtlas() {
        if (!m_proceduralAtlas) {
            [[maybe_unused]] const auto start = std::chrono::steady_clock::now();
            const Image image = sandbox::buildProceduralAtlas();
            m_proceduralAtlas =
                Texture2D::fromPixels(m_gpu->device(), image.width(), image.height(), image.data(), true);
            if (!m_proceduralAtlas) return false;
            MGE_LOG_DEBUG(
                "Sandbox", "Atlas procedural generado en ",
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count(),
                " ms");
        }
        m_atlas = &m_proceduralAtlas;
        MGE_LOG_INFO("Sandbox", "Atlas: procedural (sin imágenes)");
        return true;
    }

    // Copia un tile del atlas original sobre otro (animación de agua y lava). Tiles de 32 px, fila 0 arriba
    void copyTile(int source, int destination) {
        constexpr int kTile = 32;
        const int columns = static_cast<int>(m_atlas->width()) / kTile;
        auto x = [&](int tile) { return static_cast<std::uint32_t>((tile % columns) * kTile); };
        auto y = [&](int tile) { return static_cast<std::uint32_t>((tile / columns) * kTile); };
        m_atlas->copyFromOriginal(m_gpu->device(), x(source), y(source), kTile, kTile, x(destination),
                                  y(destination));
    }

    PlatformModule* m_platform = nullptr; // Válidos entre onInit y el cierre
    GpuModule* m_gpu = nullptr;
    AudioModule* m_audio = nullptr;
    static constexpr float kMusicVolume = 0.7f;
    bool m_musicOn = true; // N
    sandbox::SandboxSounds m_sounds;
    sandbox::RegisteredSounds m_soundIds;
    Pcg32 m_audioRng{1337, 7};
    SongId m_daySongId = kInvalidSong;
    SongId m_nightSongId = kInvalidSong;
    bool m_isNightMusic = false;

    float m_stepAccumulator = 0.0f;
    glm::vec3 m_prevFeet{0.0f};
    bool m_wasGrounded = false;
    float m_lastFallSpeed = 0.0f;
    bool m_wasInWater = false;

    static mge::ApplicationConfig makeConfig() {
        mge::ApplicationConfig config;
        config.title = "miniGameEngine - Sandbox";
        config.width = 1280;
        config.height = 720;
        return config;
    }

    std::unique_ptr<MeshRenderer> m_renderer;
    std::unique_ptr<SkyRenderer> m_skyRenderer;
    DayNight m_dayNight{9.0f, 600.0f}; // Comienza a las 9:00, día de 10 minutos (600 s)
    std::array<BlockId, 9> m_hotbarBlocks{};
    int m_selectedSlot = 0;
    bool m_showHud = true; // F11
    bool m_pngMissingLogged = false;

    using VoxelEdits =
        std::unordered_map<ChunkCoord, std::vector<std::pair<std::uint16_t, BlockId>>, ChunkCoordHash>;
    VoxelEdits m_edits;
    std::optional<VoxelRaycastHit> m_selectionHit;
    float m_breakTimer = 0.0f;
    float m_placeTimer = 0.0f;
    bool m_breakCapturedThisClick = false;
    Entity m_playerEntity; // Transform + CharacterController
    Entity m_cameraEntity; // Transform + Camera + CharacterCamera
    PhysicsWorld m_physicsWorld;
    std::deque<Entity> m_thrownBodies;
    // El mundo: catálogo de bloques del sandbox, chunks (CPU) y sus mallas (GPU)
    BlockRegistry m_registry;
    sandbox::SandboxBlocks m_blocks;
    const sandbox::SandboxTerrain* m_terrain = nullptr; // Lo posee m_world
    VoxelWorld m_world;
    std::unique_ptr<ChunkStreamer> m_streamer;
    std::map<ChunkCoord, GpuChunk> m_gpuChunks;
    RenderQueue m_queue;
    std::unique_ptr<DebugDraw> m_debug;
    std::unique_ptr<Renderer2D> m_ui;
    DebugOverlay m_overlay;       // F1
    bool m_showDebug = false;     // F3
    bool m_frustumFrozen = false; // F4
    glm::mat4 m_frozenViewProjection{1.0f};
    Texture2D m_pngAtlas;         // Minecraft-like_3d_tileset.png (se carga la primera vez que se usa)
    Texture2D m_proceduralAtlas;  // sandbox::buildProceduralAtlas (se genera la primera vez que se usa)
    Texture2D* m_atlas = nullptr; // El que se dibuja

    // Medición de FPS para el log
    int m_fpsFrames = 0;
    float m_fpsElapsed = 0.0f;

    float m_animTimer = 0.0f;
    int m_currentAnimFrame = 0;

    bool m_mouseCaptured = true;
    bool m_isPaused = false;
    bool m_lodEnabled = true; // F2
};

// Definición de la función de entrada del motor
mge::Application* mge::createApplication() {
    return new SandboxApp();
}