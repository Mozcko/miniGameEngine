#include <mge/platform/PlatformModule.h>

#include <mge/core/Application.h>
#include <mge/core/Log.h>
#include <mge/platform/SdlInput.h>

namespace mge {

    namespace {
        // Each module releases only its own subsystem; the last one out shuts down SDL completely.
        // (SDL tracks reference counts per subsystem.)
        void quitSdlSubsystem(SDL_InitFlags flags) {
            SDL_QuitSubSystem(flags);
            if (SDL_WasInit(0) == 0) SDL_Quit();
        }
    }

    bool PlatformModule::onInit(Application& app) {
        if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
            MGE_LOG_FATAL("Platform", "Failed to initialize SDL video: ", SDL_GetError());
            return false;
        }

        const ApplicationConfig& config = app.getConfig();
        m_window = std::make_unique<Window>(config.title, config.width, config.height);
        if (!m_window->isValid()) {
            m_window.reset(); // Reason is already logged
            quitSdlSubsystem(SDL_INIT_VIDEO);
            return false;
        }
        return true;
    }

    void PlatformModule::onFrameBegin(Application& app) {
        m_input.beginFrame();

        // The OS may release cursor capture (e.g., alt-tab); re-apply while requested
        if (m_cursorLocked) {
            SDL_Window* window = m_window->getSDLWindow();
            if (!SDL_GetWindowRelativeMouseMode(window)) SDL_SetWindowRelativeMouseMode(window, true);
            if (!SDL_GetWindowMouseGrab(window)) SDL_SetWindowMouseGrab(window, true);
        }

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) app.requestStop();
            if (m_eventCallback) m_eventCallback(event);
            applySdlEvent(m_input, event);
        }
    }

    void PlatformModule::setCursorLocked(bool locked) {
        m_cursorLocked = locked;
        if (!m_window) return;
        SDL_SetWindowRelativeMouseMode(m_window->getSDLWindow(), locked);
        SDL_SetWindowMouseGrab(m_window->getSDLWindow(), locked);
    }

    void PlatformModule::onShutdown(Application& /*app*/) {
        m_input.clear();
        // Window first, then the video subsystem
        m_window.reset();
        quitSdlSubsystem(SDL_INIT_VIDEO);
    }
}
