#pragma once

#include <mge/core/Module.h>
#include <mge/platform/Window.h>

#include <SDL3/SDL.h>
#include <functional>
#include <memory>

namespace mge {

    // Módulo de plataforma: crea la ventana + contexto OpenGL (con el título/tamaño del
    // ApplicationConfig), procesa los eventos de SDL cada frame y presenta la imagen al final.
    class PlatformModule : public Module {
    public:
        using EventCallback = std::function<void(const SDL_Event&)>;

        const char* name() const override { return "Platform"; }

        bool onInit(Application& app) override;
        void onFrameBegin(Application& app) override;
        void onFrameEnd(Application& app) override;
        void onShutdown(Application& app) override;

        // Recibe cada evento de SDL antes que el InputManager.
        // Temporal: en #15 los juegos usarán acciones de input en lugar de eventos crudos.
        void setEventCallback(EventCallback callback) { m_eventCallback = std::move(callback); }

        // Válido entre onInit y onShutdown
        Window& getWindow() { return *m_window; }

    private:
        std::unique_ptr<Window> m_window;
        EventCallback m_eventCallback;
    };
}
