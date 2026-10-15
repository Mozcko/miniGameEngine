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

// Mallas en GPU de un chunk (null si esa parte está vacía)
struct GpuChunk {
    std::unique_ptr<Mesh> opaque;
    std::unique_ptr<Mesh> transparent;
};

class SandboxApp : public mge::Application {
public:
    SandboxApp()
        : mge::Application(makeConfig()), m_camera(1280, 720),
          m_blocks(sandbox::registerSandboxBlocks(m_registry)), m_world(m_registry, makeTerrain()) {
        // Módulos que usa el sandbox (orden de inicialización; se cierran al revés)
        addModule<PlatformModule>();
        addModule<AudioModule>();
    }

    bool onInit() override {
        m_platform = getModule<PlatformModule>();
        // Eventos de ventana (tamaño, foco); el input del juego va por acciones
        m_platform->setEventCallback([this](const SDL_Event& event) { handleWindowEvent(event); });
        bindControls(m_platform->actions());

        // 1. Shaders
        bool shaderOk = m_shader3D.compileShaders(assetPath("shaders/basic3d.vert").string(),
                                                  assetPath("shaders/basic3d.frag").string());
        m_shader3D.addAttribute("vertexPosition");
        m_shader3D.addAttribute("vertexUV");
        shaderOk = m_shader3D.linkShaders() && shaderOk;
        if (!shaderOk) {
            MGE_LOG_FATAL("Sandbox", "No se pudo crear el shader 3D. Assets en: ", assetRoot().string(),
                          " (se puede cambiar con MGE_ASSET_DIR)");
            return false;
        }

        // 2. Cámara
        // Aparecer sobre el terreno (o sobre el agua) en el centro de la columna (0, 0)
        const int ground = std::max(m_terrain->surfaceHeight(0, 0), sandbox::SandboxTerrain::kWaterLevel);
        m_camera.setPosition(glm::vec3(0.5f, static_cast<float>(ground) + 2.5f, 0.5f));

        // Configurar Colisiones
        // Colisiones: ¿algún bloque sólido toca la caja? (el bloque (x,y,z) ocupa [x, x+1)³)
        m_camera.setCollisionCallback([this](const glm::vec3& min, const glm::vec3& max) -> bool {
            const glm::ivec3 lo = blockAt(min);
            const glm::ivec3 hi = blockAt(max);
            for (int x = lo.x; x <= hi.x; x++)
                for (int y = lo.y; y <= hi.y; y++)
                    for (int z = lo.z; z <= hi.z; z++)
                        if (m_world.isSolid({x, y, z})) return true;
            return false;
        });

        // 5. Cargar Textura
        m_tilesetTexture.load(assetPath("textures/Minecraft-like_3d_tileset.png").string());

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // Configuración inicial del ratón
        m_platform->setCursorLocked(true);

        // El HUD de texto (SDL_ttf) se quitó en #9; vuelve con la fuente embebida en M4 (#41-#43).
        // Mientras tanto los FPS se escriben en el log (nivel Debug) cada 5 segundos.
        MGE_LOG_INFO("Sandbox",
                     "WASD: mover, Espacio/Shift: subir/bajar, Ctrl: correr, P: pausa, Esc: salir");
        return true;
    }

    void onShutdown() override {
        // Liberar las mallas aquí (no en el destructor): el contexto OpenGL todavía existe
        m_gpuChunks.clear();
        // La textura no se libera sola (Texture es un handle copiable): liberarla con el contexto vivo
        m_tilesetTexture.destroy();
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

    void handleWindowEvent(const SDL_Event& event) {
        // Tamaño en píxeles (no en puntos): correcto también en pantallas HiDPI
        if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
            int newWidth = event.window.data1;
            int newHeight = event.window.data2;
            glViewport(0, 0, newWidth, newHeight);
            m_camera.updateProjection((float)newWidth, (float)newHeight);
        }

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
            m_tilesetTexture.updateTile(sandbox::kWaterTile,
                                        m_waterFrames[m_currentAnimFrame % m_waterFrames.size()], 32, 32);
            m_tilesetTexture.updateTile(sandbox::kLavaTile,
                                        m_lavaFrames[m_currentAnimFrame % m_lavaFrames.size()], 32, 32);
        }

        streamChunks();
        m_camera.update(deltaTime, cameraInput());
    }

    void onRender() override {
        glClearColor(0.5f, 0.7f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        m_shader3D.use();
        glUniformMatrix4fv(m_shader3D.getUniformLocation("projection"), 1, GL_FALSE,
                           &(m_camera.getProjectionMatrix()[0][0]));
        glUniformMatrix4fv(m_shader3D.getUniformLocation("view"), 1, GL_FALSE,
                           &(m_camera.getViewMatrix()[0][0]));
        glUniform4f(m_shader3D.getUniformLocation("baseColor"), 1.0f, 1.0f, 1.0f, 1.0f);
        glUniform1i(m_shader3D.getUniformLocation("myTexture"), 0);

        m_tilesetTexture.bind();

        const GLint modelLocation = m_shader3D.getUniformLocation("model");
        auto drawPass = [&](bool transparent) {
            for (auto& [coord, gpu] : m_gpuChunks) {
                Mesh* mesh = transparent ? gpu.transparent.get() : gpu.opaque.get();
                if (!mesh || !m_camera.isAabbVisible(chunkBounds(coord))) continue;
                const glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(chunkOrigin(coord)));
                glUniformMatrix4fv(modelLocation, 1, GL_FALSE, &model[0][0]);
                mesh->draw();
            }
        };
        drawPass(false);
        glDepthMask(GL_FALSE);
        drawPass(true);
        glDepthMask(GL_TRUE);

        m_tilesetTexture.unbind();
        m_shader3D.unuse();

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
        const ChunkCoord center = chunkOf(blockAt(m_camera.getPosition()));
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
            gpu.opaque = upload(mesh.opaque);
            gpu.transparent = upload(mesh.transparent);
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

    static std::unique_ptr<Mesh> upload(const MeshData& data) {
        if (data.empty()) return nullptr;
        auto mesh = std::make_unique<Mesh>();
        mesh->init(data);
        return mesh;
    }

    PlatformModule* m_platform = nullptr; // Válido entre onInit y el cierre

    static mge::ApplicationConfig makeConfig() {
        mge::ApplicationConfig config;
        config.title = "miniGameEngine - Sandbox";
        config.width = 1280;
        config.height = 720;
        return config;
    }

    GLSLProgram m_shader3D;
    mge::Camera3D m_camera;
    // El mundo: catálogo de bloques del sandbox, chunks (CPU) y sus mallas (GPU)
    BlockRegistry m_registry;
    sandbox::SandboxBlocks m_blocks;
    const sandbox::SandboxTerrain* m_terrain = nullptr; // Lo posee m_world
    VoxelWorld m_world;
    bool m_firstLoad = true;
    std::map<ChunkCoord, GpuChunk> m_gpuChunks;
    Texture m_tilesetTexture;

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