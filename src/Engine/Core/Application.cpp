#include "Application.h"
#include <iostream>
#include <GL/glew.h>
#include <SDL2/SDL_ttf.h>
#include "../Input/InputManager.h"

namespace Engine
{

    Application::Application() : m_Running(true), m_DeltaTime(0.0f), m_LastFrameTime(0)
    {
        // Inicializar Audio
        MiniGameEngine::AudioManager::init();

        // Inicialización de SDL
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
        {
            std::cerr << "Error al inicializar SDL: " << SDL_GetError() << std::endl;
            m_Running = false;
            return;
        }

        m_Window = std::make_unique<MiniGameEngine::Window>("Mini Game Engine - Minecraft Cube", 1280, 720);
        // Inicializar GLEW (usualmente después de crear el contexto en Window)

        if (TTF_Init() == -1)
        {
            printf("TTF_Init Error: %s\n", TTF_GetError());
        }
    }

    Application::~Application()
    {
        // Limpiar Audio
        MiniGameEngine::AudioManager::shutdown();
        TTF_Quit();
        // Limpieza
        SDL_Quit();
    }

    void Application::CalculateDeltaTime()
    {
        Uint64 currentFrameTime = SDL_GetPerformanceCounter();
        m_DeltaTime = (float)(currentFrameTime - m_LastFrameTime) / (float)SDL_GetPerformanceFrequency();
        m_LastFrameTime = currentFrameTime;
    }

    void Application::Run()
    {
        InputManager::Init(m_Window->getSDLWindow());
        OnInit();

        m_LastFrameTime = SDL_GetPerformanceCounter();

        while (m_Running)
        {
            CalculateDeltaTime();
            InputManager::Update();

            SDL_Event event;
            while (SDL_PollEvent(&event))
            {
                if (event.type == SDL_QUIT)
                {
                    m_Running = false;
                }
                OnEvent(event);
                InputManager::ProcessEvent(event);
            }

            OnUpdate(m_DeltaTime);
            OnRender();

            m_Window->swapBuffers();
        }
    }
}