#include <mge/mge.h>

#include <game/SandboxBlocks.h>
#include <game/SandboxTerrain.h>
#include <SDL3/SDL.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <map>
#include <memory>
#include <vector>

using namespace mge;

// Mallas en la GPU de un chunk (vacías si esa parte no tiene caras)
struct GpuChunk {
    GpuMesh opaque;
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

        // 3. Atlas de texturas (el camino de los assets). Se guarda una copia en la CPU para animar el agua
        // y la lava copiando tiles.
        m_atlas =
            Texture2D::fromFile(m_gpu->device(), assetPath("textures/Minecraft-like_3d_tileset.png"), true);
        if (!m_atlas) {
            MGE_LOG_FATAL("Sandbox", "No se pudo cargar el atlas. Assets en: ", assetRoot().string(),
                          " (se puede cambiar con MGE_ASSET_DIR)");
            return false;
        }

        // Configuración inicial del ratón
        m_platform->setCursorLocked(true);

        // El HUD de texto (SDL_ttf) se quitó en #9; vuelve con la fuente embebida en M4 (#41-#43).
        // Mientras tanto los FPS se escriben en el log (nivel Debug) cada 5 segundos.
        MGE_LOG_INFO("Sandbox",
                     "WASD: mover, Espacio/Shift: subir/bajar, Ctrl: correr, P: pausa, Esc: salir");
        return true;
    }

    void onShutdown() override {
        // Recursos de la GPU antes que el device (el GpuModule se cierra después del juego)
        m_gpuChunks.clear();
        m_atlas = {};
        m_renderer.reset();
    }

    // Controles del sandbox (mismas teclas que antes): cambiar un control = cambiar un binding
    static void bindControls(mge::ActionMap& actions) {
        actions.bindAxis("MoveForward", Key::W, Key::S);
        actions.bindAxis("MoveRight", Key::D, Key::A);
        actions.bindAxis("MoveUp", Key::Space, Key::LeftShift);
        actions.bind("Sprint", Key::LeftCtrl);
        actions.bind("Pause", Key::P);
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
            copyTile(m_waterFrames[m_currentAnimFrame % m_waterFrames.size()], sandbox::kWaterTile);
            copyTile(m_lavaFrames[m_currentAnimFrame % m_lavaFrames.size()], sandbox::kLavaTile);
        }

        streamChunks();
        m_fly.update(deltaTime, cameraInput());
        m_fly.apply(world().registry().get<Transform>(m_cameraEntity));
    }

    void onRender() override {
        const GpuFrame* frame = m_gpu->frame();
        if (!frame) return; // Ventana minimizada
        if (!m_renderer->begin(*frame->cmd, frame->swapchain, frame->width, frame->height,
                               {0.5f, 0.7f, 1.0f, 1.0f}))
            return;
        // La cámara del ECS: el aspecto ya está ajustado al tamaño real del swapchain (CameraSystem)
        Registry& registry = world().registry();
        const CameraMatrices camera = cameraMatrices(registry.get<Transform>(m_cameraEntity).localMatrix(),
                                                     registry.get<Camera>(m_cameraEntity), viewport());
        m_renderer->setCamera(camera.viewProjection);

        // Primero lo opaco, después lo transparente (mezcla, sin escribir profundidad)
        for (bool transparent : {false, true}) {
            for (auto& [coord, gpu] : m_gpuChunks) {
                const GpuMesh& mesh = transparent ? gpu.transparent : gpu.opaque;
                if (mesh.empty() || !camera.frustum.intersects(chunkBounds(coord))) continue;
                const glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(chunkOrigin(coord)));
                m_renderer->draw(mesh, model, &m_atlas,
                                 transparent ? MeshPass::Transparent : MeshPass::Opaque);
            }
        }
        m_renderer->end();

        // FPS promedio cada 5 s (tiempo real: en pausa el escalado es 0)
        m_fpsFrames++;
        m_fpsElapsed += time().unscaledDelta();
        if (m_fpsElapsed >= 5.0f) {
            MGE_LOG_DEBUG("Sandbox", "FPS: ", static_cast<int>(m_fpsFrames / m_fpsElapsed), " (",
                          m_gpuChunks.size(), " chunks)");
            m_fpsFrames = 0;
            m_fpsElapsed = 0.0f;
        }
    }

private:
    static constexpr int kRenderDistance = 4; // Chunks alrededor de la cámara (9×9)

    // Caja del chunk en el mundo
    static mge::Aabb chunkBounds(const ChunkCoord& coord) {
        const glm::vec3 origin(chunkOrigin(coord));
        return mge::Aabb(origin, origin + glm::vec3(static_cast<float>(kChunkSize)));
    }

    // Carga los chunks cercanos y (re)malla los sucios. Se carga un anillo más de lo que se dibuja:
    // así un chunk se malla con sus vecinos reales ya presentes y no hay caras de más en los bordes.
    // La carga es por distancia (no por frustum): girar la cámara no debe dejar huecos.
    void streamChunks() {
        using Clock = std::chrono::steady_clock;
        const auto start = Clock::now();
        const ChunkCoord center = chunkOf(blockAt(m_fly.position()));
        const int load = kRenderDistance + 1;
        const std::size_t before = m_world.chunkCount();
        // Columnas enteras: todos los chunks verticales del mundo
        for (int cx = center.x - load; cx <= center.x + load; cx++)
            for (int cz = center.z - load; cz <= center.z + load; cz++)
                for (int cy = 0; cy < sandbox::SandboxTerrain::kWorldChunksY; cy++)
                    m_world.loadChunk({cx, cy, cz});
        const std::size_t loaded = m_world.chunkCount() - before;
        const auto generated = Clock::now();
        int meshed = 0;

        ChunkMeshOptions options;
        options.atlas = {sandbox::kAtlasColumns, sandbox::kAtlasRows};
        for (const ChunkCoord& coord : m_world.dirtyChunks()) {
            if (std::abs(coord.x - center.x) > kRenderDistance ||
                std::abs(coord.z - center.z) > kRenderDistance)
                continue; // Anillo exterior: solo datos, se malla cuando entre en rango
            ChunkMesh mesh = buildChunkMesh(m_world, coord, options);
            GpuChunk& gpu = m_gpuChunks[coord];
            gpu.opaque = m_gpu->device().uploadMesh(mesh.opaque);
            gpu.transparent = m_gpu->device().uploadMesh(mesh.transparent);
            m_world.clearDirty(coord);
            meshed++;
        }

        // Tiempo de carga: la primera vez (arranque) en Info, después en Debug
        if (loaded > 0) {
            const auto ms = [](auto from, auto to) {
                return std::chrono::duration<double, std::milli>(to - from).count();
            };
            const auto done = Clock::now();
            if (m_firstLoad)
                MGE_LOG_INFO("Sandbox", loaded, " chunks generados en ", ms(start, generated), " ms, ",
                             meshed, " mallados en ", ms(generated, done), " ms");
            else
                MGE_LOG_DEBUG("Sandbox", loaded, " chunks generados en ", ms(start, generated), " ms, ",
                              meshed, " mallados en ", ms(generated, done), " ms");
            m_firstLoad = false;
        }
    }

    // El mundo es dueño del generador; el sandbox guarda un puntero para consultar alturas (spawn)
    std::unique_ptr<sandbox::SandboxTerrain> makeTerrain() {
        auto terrain = std::make_unique<sandbox::SandboxTerrain>(m_blocks);
        m_terrain = terrain.get();
        return terrain;
    }

    // Copia un tile del atlas original sobre otro (animación de agua y lava). Tiles de 32 px, fila 0 arriba
    void copyTile(int source, int destination) {
        constexpr int kTile = 32;
        const int columns = static_cast<int>(m_atlas.width()) / kTile;
        auto x = [&](int tile) { return static_cast<std::uint32_t>((tile % columns) * kTile); };
        auto y = [&](int tile) { return static_cast<std::uint32_t>((tile / columns) * kTile); };
        m_atlas.copyFromOriginal(m_gpu->device(), x(source), y(source), kTile, kTile, x(destination),
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
    bool m_firstLoad = true;
    std::map<ChunkCoord, GpuChunk> m_gpuChunks;
    Texture2D m_atlas;

    // Medición de FPS para el log
    int m_fpsFrames = 0;
    float m_fpsElapsed = 0.0f;

    std::vector<int> m_waterFrames = {205, 206, 207, 222, 223};
    std::vector<int> m_lavaFrames = {237, 238, 239, 254, 255};
    float m_animTimer = 0.0f;
    int m_currentAnimFrame = 0;

    bool m_mouseCaptured = true;
    bool m_isPaused = false;
};

// Definición de la función de entrada del motor
mge::Application* mge::createApplication() {
    return new SandboxApp();
}