#pragma once

#include <SDL3/SDL.h>
#include <memory>
#include <mge/platform/Window.h>
#include <mge/audio/AudioManager.h>

namespace mge {

    class Application {
    public:
        Application();
        virtual ~Application();

        void run();

        // Métodos que deben ser implementados por el juego (Sandbox)
        virtual void onInit() = 0;
        virtual void onUpdate(float dt) = 0;
        virtual void onRender() = 0;
        virtual void onEvent(SDL_Event& /*event*/) {}

        mge::Window& getWindow() { return *m_window; }

    protected:
        bool m_running;

        std::unique_ptr<mge::Window> m_window;

    private:
        void calculateDeltaTime();

        float m_deltaTime;
        Uint64 m_lastFrameTime;
    };

    // Esta función debe ser definida por el cliente (tu juego) para devolver su propia aplicación
    Application* createApplication();
}