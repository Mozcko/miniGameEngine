#pragma once

#include <SDL2/SDL.h>
#include <memory>
#include "../Window/Window.h"
#include "../Audio/AudioManager.h"

namespace Engine {

    class Application {
    public:
        Application();
        virtual ~Application();

        void Run();

        // Métodos que deben ser implementados por el juego (Sandbox)
        virtual void OnInit() = 0;
        virtual void OnUpdate(float dt) = 0;
        virtual void OnRender() = 0;
        virtual void OnEvent(SDL_Event& event) {}

        MiniGameEngine::Window& GetWindow() { return *m_Window; }

    protected:
        bool m_Running;
        
        std::unique_ptr<MiniGameEngine::Window> m_Window;

    private:
        void CalculateDeltaTime();

        float m_DeltaTime;
        Uint64 m_LastFrameTime;
    };

    // Esta función debe ser definida por el cliente (tu juego) para devolver su propia aplicación
    Application* CreateApplication();
}