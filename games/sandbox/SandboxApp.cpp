#include <mge/app/Application.h>
#include <mge/platform/InputManager.h>
#include <iostream>
#include <vector>
#include <map>
#include <SDL2/SDL.h>
#include <mge/core/Time.h>
#include <mge/render/core/GLSLProgram.h>
#include <mge/render/core/Texture.h>
#include <mge/render/3d/Mesh.h>
#include <mge/render/2d/GUILabel.h>
#include <mge/render/camera/Camera2D.h>
#include <mge/render/camera/Camera3D.h>
#include <sandbox/Block.h>
#include <sandbox/Chunk.h>
#include <glm/gtc/matrix_transform.hpp>

using namespace mge;

struct ChunkKey {
    int x, z;
    bool operator<(const ChunkKey& other) const { return std::tie(x, z) < std::tie(other.x, other.z); }
};

class SandboxApp : public mge::Application {
public:
    SandboxApp() : m_camera(1280, 720), m_guiCamera(1280, 720) {}

    ~SandboxApp() {
        for (auto& pair : m_chunkMap) {
            delete pair.second;
        }
    }

    void onInit() override {
        // 1. Shaders
        m_shader3D.compileShaders("../assets/shaders/basic3d.vert", "../assets/shaders/basic3d.frag");
        m_shader3D.addAttribute("vertexPosition");
        m_shader3D.addAttribute("vertexUV");
        m_shader3D.linkShaders();

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
        m_tilesetTexture.load("../assets/textures/Minecraft-like_3d_tileset.png");

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // Inicializar Tiempo
        Time::init();

        // Configuración inicial del ratón
        mge::InputManager::setCursorMode(true);

        // --- GUI SETUP ---
        // Compilar shader 2D (Usando los que ya tenías para el juego 2D)
        m_guiShader.compileShaders("../assets/shaders/texture.vert", "../assets/shaders/texture.frag");
        m_guiShader.addAttribute("vertexPosition");
        m_guiShader.addAttribute("vertexUV");
        m_guiShader.linkShaders();

        // Cámara 2D del tamaño de la ventana (para coordenadas de pixel)
        // _guiCamera ya inicializada en el constructor

        // Configurar Etiquetas
        m_fpsLabel.init("../assets/fonts/PixelOperator8-Bold.ttf", 24);
        m_fpsLabel.setPosition(10.0f, 680.0f); // Esquina superior izquierda
        m_fpsLabel.setText("FPS: 60", m_fpsColor);

        m_infoLabel.init("../assets/fonts/PixelOperator8.ttf", 16);
        m_infoLabel.setPosition(10.0f, 10.0f); // Abajo
        m_infoLabel.setText("mge v1.0 - WASD to Move", glm::vec4(1.0f));
    }

    void onEvent(SDL_Event& event) override {
        if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_RESIZED) {
            int newWidth = event.window.data1;
            int newHeight = event.window.data2;
            glViewport(0, 0, newWidth, newHeight);
            m_camera.updateProjection((float)newWidth, (float)newHeight);
        }

        if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED &&
            m_mouseCaptured && !m_isPaused) {
            mge::InputManager::setCursorMode(true);
        }

        if (event.type == SDL_MOUSEBUTTONDOWN && !m_isPaused) {
            mge::InputManager::setCursorMode(true);
        }

        if (event.type == SDL_KEYDOWN) {
            if (event.key.keysym.sym == SDLK_ESCAPE) m_running = false;

            if (event.key.keysym.sym == SDLK_p) {
                m_isPaused = !m_isPaused;
                if (m_isPaused) {
                    Time::setTimeScale(0.0f);
                    mge::InputManager::setCursorMode(false);
                } else {
                    Time::setTimeScale(1.0f);
                    mge::InputManager::setCursorMode(true);
                }
            }
        }
    }

    void onUpdate(float /*dt*/) override {
        Time::update(); // Actualizamos el sistema de tiempo global
        float deltaTime = Time::getDeltaTime();

        // Animaciones
        m_animTimer += Time::getUnscaledDeltaTime();
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

        m_camera.update(deltaTime);
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

        // 2. DIBUJAR GUI (ENCIMA DEL 3D)
        // Desactivar Depth Test para que la GUI siempre se dibuje encima
        glDisable(GL_DEPTH_TEST);

        m_guiCamera.update();

        // Actualizar FPS dinámicamente (ejemplo)
        static int frameCount = 0;
        frameCount++;
        if (frameCount % 60 == 0) {
            // Tiempo real (sin escalar): en pausa el tiempo escalado es 0 y dividiríamos entre 0
            float realDelta = Time::getUnscaledDeltaTime();
            int fps = realDelta > 0.0f ? (int)(1.0f / realDelta) : 0;
            // Pasamos el color siempre: setText usa blanco por defecto
            m_fpsLabel.setText("FPS: " + std::to_string(fps), m_fpsColor);
        }

        // Dibujar etiquetas
        m_fpsLabel.draw(m_guiShader, m_guiCamera.getProjectionMatrix());
        m_infoLabel.draw(m_guiShader, m_guiCamera.getProjectionMatrix());

        // Reactivar Depth Test para el siguiente frame 3D
        glEnable(GL_DEPTH_TEST);
    }

private:
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

    // Variables de GUI movidas dentro de la clase
    GLSLProgram m_guiShader;
    mge::Camera2D m_guiCamera;
    GUILabel m_fpsLabel;
    const glm::vec4 m_fpsColor{1.0f, 1.0f, 0.0f, 1.0f}; // Amarillo
    GUILabel m_infoLabel;

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