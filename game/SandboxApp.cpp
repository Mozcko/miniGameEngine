#include <mge/mge.h>

#include <game/SandboxBlocks.h>
#include <game/SandboxMaterials.h>
#include <game/SandboxTerrain.h>
#include <SDL3/SDL.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <deque>
#include <string>
#include <map>
#include <memory>
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
        m_audio->playSong(m_audio->addSong(songs::sandboxAmbient()), 3.0f);
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

        // 3. Atlas: el PNG (camino de los assets) o uno generado por código (camino procedural, #34). Los
        // dos con las mismas posiciones de tiles; M alterna en vivo. SANDBOX_ATLAS=procedural arranca sin
        // cargar ninguna imagen. Se guarda una copia en la CPU para animar el agua y la lava copiando tiles.
        const char* atlasMode = std::getenv("SANDBOX_ATLAS");
        const bool procedural = atlasMode && std::string(atlasMode) == "procedural";
        if (!(procedural ? useProceduralAtlas() : usePngAtlas())) return false;

        // 4. Streaming del mundo en el pool de hilos del motor
        m_streamer = std::make_unique<ChunkStreamer>(m_world, streamingSettings(), &jobs());

        // Configuración inicial del ratón
        m_platform->setCursorLocked(true);

        // F1 muestra FPS, tiempos y contadores en pantalla (fuente embebida, #43); además cada 5 s van al log
        MGE_LOG_INFO(
            "Sandbox",
            "WASD: mover, Espacio: saltar, F: caminar/volar, Espacio/Shift: subir/bajar (vuelo), "
            "Ctrl: correr, M: atlas "
            "PNG/procedural, N: música, P: pausa, F1: rendimiento, F2: LOD, F3/F4: depuración, Esc: salir");
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
        actions.bind("FastTime", Key::T);
        actions.bind("Quit", Key::Escape);
        actions.bind("CaptureMouse", MouseButton::Left);
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

        if (m_platform->wasActionPressed("CaptureMouse") && !m_isPaused) m_platform->setCursorLocked(true);

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
    }

    void onUpdate(float /*dt*/) override {
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

        // Seguridad de streaming: no simular hasta que el chunk bajo los pies y el inferior estén cargados
        const glm::vec3 feet = world().registry().get<Transform>(m_playerEntity).position;
        controller.enabled = m_world.isLoaded(chunkOf(blockAt(feet))) &&
                             m_world.isLoaded(chunkOf(blockAt(feet) - glm::ivec3(0, 1, 0)));

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
            const RenderStats& stats = m_renderer->stats();
            MGE_LOG_DEBUG("Sandbox", "FPS: ", static_cast<int>(m_fpsFrames / m_fpsElapsed), " (",
                          m_gpuChunks.size(), " chunks; dibujo: ", stats.totalDrawCalls(), " llamadas, ",
                          stats.totalTriangles(), " triángulos = opaco ", stats.triangles[0], " + cutout ",
                          stats.triangles[1], " + transparente ", stats.triangles[2], ")");
            const StreamingStats streaming = m_streamer->stats();
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

    // F1: FPS, gráfico de tiempos y contadores, encima de la escena (Renderer2D)
    void drawOverlay(const GpuFrame& frame) {
        m_overlay.addFrame(time().unscaledDelta() * 1000.0f, frame.acquireMs);
        if (!m_overlay.visible()) return;
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
                                           std::to_string(render.totalTriangles() / 1000) + "k triangulos");
        std::array<int, 3> lodChunks = {0, 0, 0};
        std::array<std::size_t, 3> lodTris = {0, 0, 0};
        for (const auto& [coord, gpu] : m_gpuChunks) {
            const int l = std::clamp(gpu.lod, 0, 2);
            lodChunks[l]++;
            lodTris[l] += (gpu.opaque.indexCount + gpu.cutout.indexCount + gpu.transparent.indexCount) / 3;
        }
        m_overlay.setCounter(
            "lod", (m_lodEnabled ? "ON" : "OFF") + std::string(" (L0: ") + std::to_string(lodChunks[0]) +
                       "c/" + std::to_string(lodTris[0] / 1000) + "k, L1: " + std::to_string(lodChunks[1]) +
                       "c/" + std::to_string(lodTris[1] / 1000) + "k, L2: " + std::to_string(lodChunks[2]) +
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
        m_overlay.setCounter("atlas", m_atlas == &m_pngAtlas ? "PNG" : "procedural");
        m_overlay.setCounter("cuerpos", std::to_string(m_thrownBodies.size()) + " (" +
                                            std::to_string(m_physicsWorld.lastPairCount()) + " pares)");
        m_overlay.setCounter(
            "", "F1 panel  F2 LOD  F3 chunks  F4 frustum  F volar  G lanzar  M atlas  N musica  P pausa");

        m_overlay.setProfile(profiler());

        // Escala entera: la fuente 8x8 queda nítida en pantallas HiDPI
        {
            MGE_PROFILE_SCOPE(profiler(), "UI");
            m_ui->begin(frame.width, frame.height, std::max(1.0f, std::round(frame.displayScale)));
            m_overlay.draw(*m_ui);
            m_ui->flush(*frame.cmd, frame.swapchain);
        }
    }

    // F3: bordes de los chunks (verde = mallado, amarillo = con datos sin malla, rojo = sucio), la caja de
    // colisión de la cámara y, con F4, el frustum congelado
    void drawDebug() {
        if (!m_showDebug && !m_frustumFrozen && m_thrownBodies.empty()) return;
        m_debug->clear();
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
            m_pngAtlas = Texture2D::fromFile(m_gpu->device(),
                                             assetPath("textures/Minecraft-like_3d_tileset.png"), true);
            if (!m_pngAtlas) {
                MGE_LOG_ERROR("Sandbox", "No se pudo cargar el atlas. Assets en: ", assetRoot().string(),
                              " (se puede cambiar con MGE_ASSET_DIR)");
                return false;
            }
        }
        m_atlas = &m_pngAtlas;
        MGE_LOG_INFO("Sandbox", "Atlas: PNG (assets)");
        return true;
    }

    bool useProceduralAtlas() {
        if (!m_proceduralAtlas) {
            const auto start = std::chrono::steady_clock::now();
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

    static mge::ApplicationConfig makeConfig() {
        mge::ApplicationConfig config;
        config.title = "miniGameEngine - Sandbox";
        config.width = 1280;
        config.height = 720;
        return config;
    }

    std::unique_ptr<MeshRenderer> m_renderer;
    std::unique_ptr<SkyRenderer> m_skyRenderer;
    DayNight m_dayNight;
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