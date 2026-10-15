#include <mge/app/Application.h>
#include <GL/glew.h>
#include <SDL2/SDL_ttf.h>
#include <mge/platform/InputManager.h>
#include <mge/core/Log.h>

namespace mge {

    Application::Application() : m_running(true), m_deltaTime(0.0f), m_lastFrameTime(0) {
        // Inicializar Audio
        mge::AudioManager::init();

        // Inicialización de SDL
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
            MGE_LOG_FATAL("App", "No se pudo inicializar SDL: ", SDL_GetError());
            m_running = false;
            return;
        }

        m_window = std::make_unique<mge::Window>("Mini Game Engine - Minecraft Cube", 1280, 720);
        if (!m_window->isValid()) {
            m_running = false; // El motivo ya está en el log
            return;
        }

        if (TTF_Init() == -1) {
            MGE_LOG_ERROR("App", "No se pudo inicializar SDL_ttf: ", TTF_GetError());
        }
    }

    Application::~Application() {
        // Limpiar Audio
        mge::AudioManager::shutdown();
        TTF_Quit();
        // Limpieza
        SDL_Quit();
    }

    void Application::calculateDeltaTime() {
        Uint64 currentFrameTime = SDL_GetPerformanceCounter();
        m_deltaTime = (float)(currentFrameTime - m_lastFrameTime) / (float)SDL_GetPerformanceFrequency();
        m_lastFrameTime = currentFrameTime;
    }

    void Application::run() {
        // Si la inicialización falló no hay ventana/contexto: no llamar a onInit (usaría OpenGL)
        if (!m_running || !m_window || !m_window->isValid()) {
            MGE_LOG_FATAL("App", "La aplicación no se pudo inicializar; saliendo");
            return;
        }

        InputManager::init(m_window->getSDLWindow());
        onInit();

        m_lastFrameTime = SDL_GetPerformanceCounter();

        while (m_running) {
            calculateDeltaTime();
            InputManager::update();

            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) {
                    m_running = false;
                }
                onEvent(event);
                InputManager::processEvent(event);
            }

            onUpdate(m_deltaTime);
            onRender();

            m_window->swapBuffers();
        }
    }
}