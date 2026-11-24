#pragma once

#include <mge/core/Input.h>
#include <mge/core/Module.h>
#include <mge/platform/Window.h>

#include <SDL3/SDL.h>
#include <functional>
#include <memory>

namespace mge {

    // Platform module: creates the window (with title/size from ApplicationConfig) and translates
    // SDL3 events into core input each frame. To render, add a GpuModule afterwards.
    class PlatformModule : public Module {
    public:
        using EventCallback = std::function<void(const SDL_Event&)>;

        const char* name() const override { return "Platform"; }

        bool onInit(Application& app) override;
        void onFrameBegin(Application& app) override;
        void onShutdown(Application& app) override;

        // Current frame input (SDL-independent) and game actions
        InputState& input() { return m_input; }
        const InputState& input() const { return m_input; }
        ActionMap& actions() { return m_actions; }
        const ActionMap& actions() const { return m_actions; }

        // Shortcuts: ActionMap action evaluated against the current input
        bool isActionDown(const std::string& action) const { return m_actions.isDown(m_input, action); }
        bool wasActionPressed(const std::string& action) const {
            return m_actions.wasPressed(m_input, action);
        }
        bool wasActionReleased(const std::string& action) const {
            return m_actions.wasReleased(m_input, action);
        }
        float axis(const std::string& axisName) const { return m_actions.axis(m_input, axisName); }

        // Locked cursor: hidden, relative motion, and confined to the window (FPS cameras)
        void setCursorLocked(bool locked);
        bool isCursorLocked() const { return m_cursorLocked; }

        // Raw SDL events for non-input handling (e.g., window resize).
        // Called before updating input.
        void setEventCallback(EventCallback callback) { m_eventCallback = std::move(callback); }

        // Valid between onInit and onShutdown
        Window& getWindow() { return *m_window; }

    private:
        std::unique_ptr<Window> m_window;
        EventCallback m_eventCallback;
        InputState m_input;
        ActionMap m_actions;
        bool m_cursorLocked = false;
    };
}
