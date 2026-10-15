#include <mge/mge.h>

#include <game/Block.h>
#include <game/Chunk.h>
#include <SDL3/SDL.h>
#include <glm/gtc/matrix_transform.hpp>
#include <map>
#include <vector>

using namespace mge;

struct ChunkKey {
    int x, z;
    bool operator<(const ChunkKey& other) const { return std::tie(x, z) < std::tie(other.x, other.z); }
};

class SandboxApp : public mge::Application {
public:
    SandboxApp() : mge::Application(makeConfig()), m_camera(1280, 720) {
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
        m_camera.setPosition(glm::vec3(0.0f, 25.0f, 0.0f));

        // Configurar Colisiones
        m_camera.setCollisionCallback([this](const glm::vec3& min, const glm::vec3& max) -> bool {
            int minX = floor(min.x + 0.5f);
            int maxX = floor(max.x + 0.5f);
            int minY = floor(min.y + 0.5f);
            int maxY = floor(max.y + 0.5f);
            int minZ = floor(min.z + 0.5f);
            int maxZ = floor(max.z + 0.5f);

            for (int x = minX; x <= maxX; x++) {
                for (int y = minY; y <= maxY; y++) {
                    for (int z = minZ; z <= maxZ; z++) {
                        if (getGlobalBlock(x, y, z).isSolid()) return true;
                    }
                }
            }
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
        // Liberar los chunks aquí (no en el destructor): el contexto OpenGL todavía existe
        for (auto& pair : m_chunkMap) {
            delete pair.second;
        }
        m_chunkMap.clear();
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
            m_tilesetTexture.updateTile(m_waterTarget,
                                        m_waterFrames[m_currentAnimFrame % m_waterFrames.size()], 32, 32);
            m_tilesetTexture.updateTile(m_lavaTarget, m_lavaFrames[m_currentAnimFrame % m_lavaFrames.size()],
                                        32, 32);
        }

        // Actualizar Chunks
        int pChunkX = (int)floor(m_camera.getPosition().x / kChunkSize);
        int pChunkZ = (int)floor(m_camera.getPosition().z / kChunkSize);
        int renderDist = 4;

        for (int cx = pChunkX - renderDist; cx <= pChunkX + renderDist; cx++) {
            for (int cz = pChunkZ - renderDist; cz <= pChunkZ + renderDist; cz++) {
                glm::vec3 chunkCenter((cx * kChunkSize) + kChunkSize / 2.0f, kChunkHeight / 2.0f,
                                      (cz * kChunkSize) + kChunkSize / 2.0f);
                if (m_camera.isSphereVisible(chunkCenter, 14.0f)) {
                    ChunkKey key{cx, cz};
                    if (m_chunkMap.find(key) == m_chunkMap.end()) {
                        Chunk* chunk = new Chunk(cx, cz);
                        chunk->buildMesh();
                        m_chunkMap[key] = chunk;
                    }
                }
            }
        }

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

        for (auto& pair : m_chunkMap) {
            if (m_camera.isSphereVisible(pair.second->getCenter(), pair.second->getRadius()))
                pair.second->drawOpaque(m_shader3D);
        }

        glDepthMask(GL_FALSE);
        for (auto& pair : m_chunkMap) {
            if (m_camera.isSphereVisible(pair.second->getCenter(), pair.second->getRadius()))
                pair.second->drawTransparent(m_shader3D);
        }
        glDepthMask(GL_TRUE);

        m_tilesetTexture.unbind();
        m_shader3D.unuse();

        // FPS promedio cada 5 s (tiempo real: en pausa el escalado es 0)
        m_fpsFrames++;
        m_fpsElapsed += time().unscaledDelta();
        if (m_fpsElapsed >= 5.0f) {
            MGE_LOG_DEBUG("Sandbox", "FPS: ", static_cast<int>(m_fpsFrames / m_fpsElapsed), " (",
                          m_chunkMap.size(), " chunks)");
            m_fpsFrames = 0;
            m_fpsElapsed = 0.0f;
        }
    }

private:
    PlatformModule* m_platform = nullptr; // Válido entre onInit y el cierre

    static mge::ApplicationConfig makeConfig() {
        mge::ApplicationConfig config;
        config.title = "miniGameEngine - Sandbox";
        config.width = 1280;
        config.height = 720;
        return config;
    }

    Block getGlobalBlock(int x, int y, int z) {
        int chunkX = (int)floor((float)x / kChunkSize);
        int chunkZ = (int)floor((float)z / kChunkSize);
        ChunkKey key{chunkX, chunkZ};
        if (m_chunkMap.find(key) != m_chunkMap.end()) {
            int localX = x % kChunkSize;
            int localZ = z % kChunkSize;
            if (localX < 0) localX += kChunkSize;
            if (localZ < 0) localZ += kChunkSize;
            return m_chunkMap[key]->getBlock(localX, y, localZ);
        }
        return Block(BlockType::Air);
    }

    GLSLProgram m_shader3D;
    mge::Camera3D m_camera;
    std::map<ChunkKey, Chunk*> m_chunkMap;
    Texture m_tilesetTexture;

    // Medición de FPS para el log
    int m_fpsFrames = 0;
    float m_fpsElapsed = 0.0f;

    std::vector<int> m_waterFrames = {205, 206, 207, 222, 223};
    std::vector<int> m_lavaFrames = {237, 238, 239, 254, 255};
    int m_waterTarget = 207;
    int m_lavaTarget = 239;
    float m_animTimer = 0.0f;
    int m_currentAnimFrame = 0;

    bool m_mouseCaptured = true;
    bool m_isPaused = false;
};

// Definición de la función de entrada del motor
mge::Application* mge::createApplication() {
    return new SandboxApp();
}