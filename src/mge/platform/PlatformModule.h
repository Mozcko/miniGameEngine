#pragma once

#include <mge/core/Input.h>
#include <mge/core/Module.h>
#include <mge/platform/Window.h>

#include <SDL3/SDL.h>
#include <functional>
#include <memory>

namespace mge {

    struct PlatformOptions {
        // true: contexto OpenGL en la ventana (renderer legacy, hasta #40).
        // false: ventana sola, para el GpuModule (SDL3 GPU).
        bool openGl = true;
    };

    // Módulo de plataforma: crea la ventana (con el título/tamaño del ApplicationConfig) y, si se pide,
    // un contexto OpenGL; traduce los eventos de SDL3 al input del core cada frame y presenta la imagen.
    class PlatformModule : public Module {
    public:
        using EventCallback = std::function<void(const SDL_Event&)>;

        explicit PlatformModule(PlatformOptions options = {}) : m_options(options) {}

        const char* name() const override { return "Platform"; }

        bool onInit(Application& app) override;
        void onFrameBegin(Application& app) override;
        void onFrameEnd(Application& app) override;
        void onShutdown(Application& app) override;

        // Input del frame actual (independiente de SDL) y las acciones del juego
        InputState& input() { return m_input; }
        const InputState& input() const { return m_input; }
        ActionMap& actions() { return m_actions; }
        const ActionMap& actions() const { return m_actions; }

        // Atajos: acción del ActionMap evaluada sobre el input actual
        bool isActionDown(const std::string& action) const { return m_actions.isDown(m_input, action); }
        bool wasActionPressed(const std::string& action) const {
            return m_actions.wasPressed(m_input, action);
        }
        float axis(const std::string& axisName) const { return m_actions.axis(m_input, axisName); }

        // Cursor capturado: oculto, movimiento relativo y confinado a la ventana (cámaras FPS)
        void setCursorLocked(bool locked);
        bool isCursorLocked() const { return m_cursorLocked; }

        // Eventos crudos de SDL para lo que no es input (ej: cambio de tamaño de la ventana).
        // Se llama antes de actualizar el input.
        void setEventCallback(EventCallback callback) { m_eventCallback = std::move(callback); }

        // Válido entre onInit y onShutdown
        Window& getWindow() { return *m_window; }
        bool hasGlContext() const { return m_window && m_window->hasGlContext(); }

    private:
        PlatformOptions m_options;
        std::unique_ptr<Window> m_window;
        EventCallback m_eventCallback;
        InputState m_input;
        ActionMap m_actions;
        bool m_cursorLocked = false;
    };
}
