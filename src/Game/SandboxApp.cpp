#include <Engine/Core/Application.h>
#include <Engine/Input/InputManager.h>
#include <iostream>
#include <vector>
#include <map>
#include <SDL2/SDL.h>
#include "../Engine/Core/Time.h"
#include "../Engine/Graphics/Core/GLSLProgram.h"
#include "../Engine/Graphics/Core/Texture.h"
#include "../Engine/Graphics/3D/Mesh.h"
#include "../Engine/Graphics/2D/GUILabel.h"
#include "../Engine/Graphics/Camera/Camera2D.h"
#include <Engine/Graphics/Camera/Camera3D.h>
#include "Block.h"
#include "Chunk.h"
#include <glm/gtc/matrix_transform.hpp>

using namespace MiniGameEngine;

struct ChunkKey
{
    int x, z;
    bool operator<(const ChunkKey &other) const
    {
        return std::tie(x, z) < std::tie(other.x, other.z);
    }
};

class SandboxApp : public Engine::Application
{
public:
    SandboxApp()
        : camera(1280, 720), _guiCamera(1280, 720)
    {
    }

    ~SandboxApp()
    {
        for (auto &pair : chunkMap)
        {
            delete pair.second;
        }
    }

    void OnInit() override
    {
        // 1. Shaders
        shader3D.compileShaders("../assets/shaders/basic3d.vert", "../assets/shaders/basic3d.frag");
        shader3D.addAttribute("vertexPosition");
        shader3D.addAttribute("vertexUV");
        shader3D.linkShaders();

        // 2. Cámara
        camera.setPosition(glm::vec3(0.0f, 25.0f, 0.0f));

        // Configurar Colisiones
        camera.setCollisionCallback([this](const glm::vec3 &min, const glm::vec3 &max) -> bool
                                    {
            int minX = floor(min.x + 0.5f); int maxX = floor(max.x + 0.5f);
            int minY = floor(min.y + 0.5f); int maxY = floor(max.y + 0.5f);
            int minZ = floor(min.z + 0.5f); int maxZ = floor(max.z + 0.5f);

            for (int x = minX; x <= maxX; x++) {
                for (int y = minY; y <= maxY; y++) {
                    for (int z = minZ; z <= maxZ; z++) {
                        if (getGlobalBlock(x, y, z).isSolid()) return true;
                    }
                }
            }
            return false; });

        // 5. Cargar Textura
        tilesetTexture.load("../assets/textures/Minecraft-like_3d_tileset.png");

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // Inicializar Tiempo
        Time::init();

        // Configuración inicial del ratón
        Engine::InputManager::SetCursorMode(true);

        // --- GUI SETUP ---
        // Compilar shader 2D (Usando los que ya tenías para el juego 2D)
        _guiShader.compileShaders("../assets/shaders/texture.vert", "../assets/shaders/texture.frag");
        _guiShader.addAttribute("vertexPosition");
        _guiShader.addAttribute("vertexUV");
        _guiShader.linkShaders();

        // Cámara 2D del tamaño de la ventana (para coordenadas de pixel)
        // _guiCamera ya inicializada en el constructor

        // Configurar Etiquetas
        _fpsLabel.init("../assets/fonts/PixelOperator8-Bold.ttf", 24);
        _fpsLabel.setPosition(10.0f, 680.0f);                            // Esquina superior izquierda
        _fpsLabel.setText("FPS: 60", glm::vec4(1.0f, 1.0f, 0.0f, 1.0f)); // Amarillo

        _infoLabel.init("../assets/fonts/PixelOperator8.ttf", 16);
        _infoLabel.setPosition(10.0f, 10.0f); // Abajo
        _infoLabel.setText("MiniGameEngine v1.0 - WASD to Move", glm::vec4(1.0f));
    }

    void OnEvent(SDL_Event &event) override
    {
        if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_RESIZED)
        {
            int newWidth = event.window.data1;
            int newHeight = event.window.data2;
            glViewport(0, 0, newWidth, newHeight);
            camera.updateProjection((float)newWidth, (float)newHeight);
        }

        if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED && mouseCaptured && !isPaused)
        {
            Engine::InputManager::SetCursorMode(true);
        }

        if (event.type == SDL_MOUSEBUTTONDOWN && !isPaused)
        {
            Engine::InputManager::SetCursorMode(true);
        }

        if (event.type == SDL_KEYDOWN)
        {
            if (event.key.keysym.sym == SDLK_ESCAPE)
                m_Running = false;

            if (event.key.keysym.sym == SDLK_p)
            {
                isPaused = !isPaused;
                if (isPaused)
                {
                    Time::setTimeScale(0.0f);
                    Engine::InputManager::SetCursorMode(false);
                }
                else
                {
                    Time::setTimeScale(1.0f);
                    Engine::InputManager::SetCursorMode(true);
                }
            }
        }
    }

    void OnUpdate(float dt) override
    {
        Time::update(); // Actualizamos el sistema de tiempo global
        float deltaTime = Time::getDeltaTime();

        // Animaciones
        animTimer += Time::getUnscaledDeltaTime();
        if (animTimer >= 0.2f)
        {
            animTimer = 0.0f;
            currentAnimFrame++;
            tilesetTexture.updateTile(waterTarget, waterFrames[currentAnimFrame % waterFrames.size()], 32, 32);
            tilesetTexture.updateTile(lavaTarget, lavaFrames[currentAnimFrame % lavaFrames.size()], 32, 32);
        }

        // Actualizar Chunks
        int pChunkX = (int)floor(camera.getPosition().x / CHUNK_SIZE);
        int pChunkZ = (int)floor(camera.getPosition().z / CHUNK_SIZE);
        int renderDist = 4;

        for (int cx = pChunkX - renderDist; cx <= pChunkX + renderDist; cx++)
        {
            for (int cz = pChunkZ - renderDist; cz <= pChunkZ + renderDist; cz++)
            {
                glm::vec3 chunkCenter((cx * CHUNK_SIZE) + CHUNK_SIZE / 2.0f, CHUNK_HEIGHT / 2.0f, (cz * CHUNK_SIZE) + CHUNK_SIZE / 2.0f);
                if (camera.isSphereVisible(chunkCenter, 14.0f))
                {
                    ChunkKey key{cx, cz};
                    if (chunkMap.find(key) == chunkMap.end())
                    {
                        chunkMap[key] = new Chunk(cx, cz);
                    }
                }
            }
        }

        camera.update(deltaTime);
    }

    void OnRender() override
    {
        glClearColor(0.5f, 0.7f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader3D.use();
        glUniformMatrix4fv(shader3D.getUniformLocation("projection"), 1, GL_FALSE, &(camera.getProjectionMatrix()[0][0]));
        glUniformMatrix4fv(shader3D.getUniformLocation("view"), 1, GL_FALSE, &(camera.getViewMatrix()[0][0]));
        glUniform4f(shader3D.getUniformLocation("baseColor"), 1.0f, 1.0f, 1.0f, 1.0f);
        glUniform1i(shader3D.getUniformLocation("myTexture"), 0);

        tilesetTexture.bind();

        for (auto &pair : chunkMap)
        {
            if (camera.isSphereVisible(pair.second->getCenter(), pair.second->getRadius()))
                pair.second->drawOpaque(shader3D);
        }

        glDepthMask(GL_FALSE);
        for (auto &pair : chunkMap)
        {
            if (camera.isSphereVisible(pair.second->getCenter(), pair.second->getRadius()))
                pair.second->drawTransparent(shader3D);
        }
        glDepthMask(GL_TRUE);

        tilesetTexture.unbind();
        shader3D.unuse();

        // 2. DIBUJAR GUI (ENCIMA DEL 3D)
        // Desactivar Depth Test para que la GUI siempre se dibuje encima
        glDisable(GL_DEPTH_TEST);

        _guiCamera.update();

        // Actualizar FPS dinámicamente (ejemplo)
        static int frameCount = 0;
        frameCount++;
        if (frameCount % 60 == 0)
        {
            _fpsLabel.setText("FPS: " + std::to_string((int)(1.0f / Time::getDeltaTime())));
        }

        // Dibujar etiquetas
        _fpsLabel.draw(_guiShader, _guiCamera.getProjectionMatrix());
        _infoLabel.draw(_guiShader, _guiCamera.getProjectionMatrix());

        // Reactivar Depth Test para el siguiente frame 3D
        glEnable(GL_DEPTH_TEST);
    }

private:
    Block getGlobalBlock(int x, int y, int z)
    {
        int chunkX = (int)floor((float)x / CHUNK_SIZE);
        int chunkZ = (int)floor((float)z / CHUNK_SIZE);
        ChunkKey key{chunkX, chunkZ};
        if (chunkMap.find(key) != chunkMap.end())
        {
            int localX = x % CHUNK_SIZE;
            int localZ = z % CHUNK_SIZE;
            if (localX < 0)
                localX += CHUNK_SIZE;
            if (localZ < 0)
                localZ += CHUNK_SIZE;
            return chunkMap[key]->getBlock(localX, y, localZ);
        }
        return Block(BlockType::Air);
    }

    GLSLProgram shader3D;
    Engine::Camera3D camera;
    std::map<ChunkKey, Chunk *> chunkMap;
    Texture tilesetTexture;

    // Variables de GUI movidas dentro de la clase
    GLSLProgram _guiShader;
    Engine::Camera2D _guiCamera;
    GUILabel _fpsLabel;
    GUILabel _infoLabel;

    std::vector<int> waterFrames = {205, 206, 207, 222, 223};
    std::vector<int> lavaFrames = {237, 238, 239, 254, 255};
    int waterTarget = 207;
    int lavaTarget = 239;
    float animTimer = 0.0f;
    int currentAnimFrame = 0;

    bool mouseCaptured = true;
    bool isPaused = false;
};

// Definición de la función de entrada del motor
Engine::Application *Engine::CreateApplication()
{
    return new SandboxApp();
}