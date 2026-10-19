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
        m_renderer = std::make_unique<MeshRenderer>(m_gpu->device(), m_gpu->swapchainFormat());
        if (!m_renderer->valid()) return false;
        m_debug = std::make_unique<DebugDraw>(m_gpu->device());
        // SANDBOX_DEBUG=1: arrancar con F3 y F4 activos (lo usa un smoke test: dibuja las líneas en CI)
        if (const char* debug = std::getenv("SANDBOX_DEBUG"); debug && std::string(debug) == "1") {
            m_showDebug = true;
            m_frustumFrozen = true;
        }
        // Sol, ambiente cielo/suelo y niebla del color del cielo, que termina en el borde de lo que se dibuja
        Lighting lighting;
        lighting.fogColor = kSkyColor;
        lighting.fogStart = (kRenderDistance - 1.5f) * kChunkSize;
        lighting.fogEnd = kRenderDistance * kChunkSize;
        m_renderer->setLighting(lighting);

        // 2. Cámara
        // Aparecer sobre el terreno (o sobre el agua) en el centro de la columna (0, 0)
        const int ground = std::max(m_terrain->surfaceHeight(0, 0), sandbox::SandboxTerrain::kWaterLevel);
        m_fly.setPosition(glm::vec3(0.5f, static_cast<float>(ground) + 2.5f, 0.5f));
        // Entidad cámara: Transform (lo escribe el FlyController) + Camera (el aspecto lo ajusta el motor)
        m_cameraEntity = world().registry().create();
        world().registry().add<Transform>(m_cameraEntity);
        world().registry().add<Camera>(m_cameraEntity);
        m_fly.apply(world().registry().get<Transform>(m_cameraEntity));

        // Configurar Colisiones
        // Colisiones: ¿algún bloque sólido toca la caja? (el bloque (x,y,z) ocupa [x, x+1)³)
        m_fly.setCollisionCallback([this](const glm::vec3& min, const glm::vec3& max) -> bool {
            const glm::ivec3 lo = blockAt(min);
            const glm::ivec3 hi = blockAt(max);
            for (int x = lo.x; x <= hi.x; x++)
                for (int y = lo.y; y <= hi.y; y++)
                    for (int z = lo.z; z <= hi.z; z++)
                        if (m_world.isSolid({x, y, z})) return true;
            return false;
        });

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

        // El HUD de texto (SDL_ttf) se quitó en #9; vuelve con la fuente embebida en M4 (#41-#43).
        // Mientras tanto los FPS se escriben en el log (nivel Debug) cada 5 segundos.
        MGE_LOG_INFO("Sandbox", "WASD: mover, Espacio/Shift: subir/bajar, Ctrl: correr, M: atlas "
                                "PNG/procedural, P: pausa, Esc: salir");
        return true;
    }

    void onShutdown() override {
        // Recursos de la GPU antes que el device (el GpuModule se cierra después del juego)
        m_streamer.reset(); // Los trabajos ya terminaron (run() los espera antes de onShutdown)
        m_gpuChunks.clear();
        m_pngAtlas = {};
        m_proceduralAtlas = {};
        m_atlas = nullptr;
        m_debug.reset();
        m_renderer.reset();
    }

    // Controles del sandbox (mismas teclas que antes): cambiar un control = cambiar un binding
    static void bindControls(mge::ActionMap& actions) {
        actions.bindAxis("MoveForward", Key::W, Key::S);
        actions.bindAxis("MoveRight", Key::D, Key::A);
        actions.bindAxis("MoveUp", Key::Space, Key::LeftShift);
        actions.bind("Sprint", Key::LeftCtrl);
        actions.bind("Pause", Key::P);
        actions.bind("ToggleAtlas", Key::M);
        actions.bind("DebugDraw", Key::F3);
        actions.bind("FreezeFrustum", Key::F4);
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

    // Traduce acciones y ratón a la intención de movimiento de la cámara
    mge::CameraInput cameraInput() const {
        mge::CameraInput input;
        input.moveForward = m_platform->axis("MoveForward");
        input.moveRight = m_platform->axis("MoveRight");
        input.moveUp = m_platform->axis("MoveUp");
        input.sprint = m_platform->isActionDown("Sprint");
        // Solo mirar con el ratón si está capturado
        if (m_platform->isCursorLocked()) {
            input.lookX = m_platform->input().mouseDelta().x;
            input.lookY = m_platform->input().mouseDelta().y;
        }
        return input;
    }

    // dt es el tiempo escalado (0 en pausa). El sandbox todavía no tiene simulación de paso fijo:
    // cámara y streaming de chunks corren una vez por frame.
    void onUpdate(float dt) override {
        float deltaTime = dt;
        handleActions();

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

        streamChunks();
        m_fly.update(deltaTime, cameraInput());
        m_fly.apply(world().registry().get<Transform>(m_cameraEntity));
    }

    void onRender() override {
        const GpuFrame* frame = m_gpu->frame();
        if (!frame) return; // Ventana minimizada
        if (!m_renderer->begin(*frame->cmd, frame->swapchain, frame->width, frame->height,
                               glm::vec4(kSkyColor, 1.0f)))
            return;
        // La cámara del ECS: el aspecto ya está ajustado al tamaño real del swapchain (CameraSystem)
        Registry& registry = world().registry();
        const CameraMatrices camera = cameraMatrices(registry.get<Transform>(m_cameraEntity).localMatrix(),
                                                     registry.get<Camera>(m_cameraEntity), viewport());
        m_renderer->setCamera(camera.viewProjection, camera.position);
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
        m_queue.sort(camera.position, m_fly.forward());
        m_renderer->submit(m_queue);
        drawDebug();
        m_renderer->end();

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
                          streaming.discarded, "; hilo principal: máx ", m_streamWorstMs, " ms/frame");
            m_streamWorstMs = 0.0;
            m_fpsFrames = 0;
            m_fpsElapsed = 0.0f;
        }
    }

private:
    static constexpr int kRenderDistance = 4;               // Chunks alrededor de la cámara (9×9)
    static constexpr glm::vec3 kSkyColor{0.5f, 0.7f, 1.0f}; // Fondo y niebla

    // Caja del chunk en el mundo
    static mge::Aabb chunkBounds(const ChunkCoord& coord) {
        const glm::vec3 origin(chunkOrigin(coord));
        return mge::Aabb(origin, origin + glm::vec3(static_cast<float>(kChunkSize)));
    }

    // Streaming (#56, #57): el ChunkStreamer genera y malla en el pool de hilos, de lo más cercano a lo más
    // lejano, y descarga lo que queda lejos (con histéresis). Acá solo se suben las mallas listas a la GPU
    // (como mucho maxResultsPerUpdate por frame) y se liberan las de los chunks descargados.
    void streamChunks() {
        const auto start = std::chrono::steady_clock::now();
        const ChunkCoord center = chunkOf(blockAt(m_fly.position()));
        StreamUpdate update = m_streamer->update(center);
        for (auto& [coord, mesh] : update.meshes) {
            GpuChunk& gpu = m_gpuChunks[coord];
            gpu.opaque = m_gpu->device().uploadMesh(mesh.opaque);
            gpu.cutout = m_gpu->device().uploadMesh(mesh.cutout);
            gpu.transparent = m_gpu->device().uploadMesh(mesh.transparent);
        }
        for (const ChunkCoord& coord : update.unloaded) m_gpuChunks.erase(coord);
        // El peor frame del intervalo (debería ser de pocos ms: el trabajo pesado va en los hilos)
        const double ms =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        m_streamWorstMs = std::max(m_streamWorstMs, ms);
    }

    StreamingSettings streamingSettings() const {
        StreamingSettings settings;
        settings.loadRadius = kRenderDistance + 1; // Un anillo más que lo que se malla
        settings.meshRadius = kRenderDistance;
        settings.unloadMargin = 2;
        settings.minChunkY = 0;
        settings.maxChunkY = sandbox::SandboxTerrain::kWorldChunksY - 1;
        settings.maxJobsInFlight = 64;
        settings.maxResultsPerUpdate = 12; // Subidas a la GPU por frame: acota el trabajo del hilo principal
        settings.meshOptions.atlas = {sandbox::kAtlasColumns, sandbox::kAtlasRows};
        settings.meshOptions.ambientOcclusion = true; // Rincones más oscuros (color de vértice)
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

    // F3: bordes de los chunks (verde = mallado, amarillo = con datos sin malla, rojo = sucio), la caja de
    // colisión de la cámara y, con F4, el frustum congelado
    void drawDebug() {
        if (!m_showDebug && !m_frustumFrozen) return;
        m_debug->clear();
        if (m_showDebug) {
            const ChunkCoord center = chunkOf(blockAt(m_fly.position()));
            for (const ChunkCoord& coord : m_world.loadedChunks()) {
                if (std::abs(coord.x - center.x) > kRenderDistance ||
                    std::abs(coord.z - center.z) > kRenderDistance)
                    continue;
                const glm::vec4 color = m_world.isDirty(coord)     ? glm::vec4(1.0f, 0.25f, 0.2f, 0.9f)
                                        : m_gpuChunks.count(coord) ? glm::vec4(0.3f, 1.0f, 0.4f, 0.5f)
                                                                   : glm::vec4(1.0f, 0.9f, 0.2f, 0.7f);
                m_debug->aabb(chunkBounds(coord), color);
            }
            const glm::vec3 p = m_fly.position();
            m_debug->aabb(Aabb(p - m_fly.halfExtents, p + m_fly.halfExtents), {1.0f, 1.0f, 1.0f, 1.0f});
        }
        if (m_frustumFrozen) m_debug->frustum(m_frozenViewProjection, {1.0f, 0.5f, 0.1f, 1.0f});
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

    static mge::ApplicationConfig makeConfig() {
        mge::ApplicationConfig config;
        config.title = "miniGameEngine - Sandbox";
        config.width = 1280;
        config.height = 720;
        return config;
    }

    std::unique_ptr<MeshRenderer> m_renderer;
    FlyController m_fly;   // Mueve la cámara (vuelo libre con colisiones)
    Entity m_cameraEntity; // Transform + Camera
    // El mundo: catálogo de bloques del sandbox, chunks (CPU) y sus mallas (GPU)
    BlockRegistry m_registry;
    sandbox::SandboxBlocks m_blocks;
    const sandbox::SandboxTerrain* m_terrain = nullptr; // Lo posee m_world
    VoxelWorld m_world;
    std::unique_ptr<ChunkStreamer> m_streamer;
    double m_streamWorstMs = 0.0; // Peor tiempo de streamChunks desde el último log
    std::map<ChunkCoord, GpuChunk> m_gpuChunks;
    RenderQueue m_queue;
    std::unique_ptr<DebugDraw> m_debug;
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
};

// Definición de la función de entrada del motor
mge::Application* mge::createApplication() {
    return new SandboxApp();
}