#include <mge/platform/PlatformModule.h>

#include <mge/core/Application.h>
#include <mge/core/Log.h>
#include <mge/platform/InputManager.h>

namespace mge {

    bool PlatformModule::onInit(Application& app) {
        const ApplicationConfig& config = app.getConfig();
        m_window = std::make_unique<Window>(config.title, config.width, config.height);
        if (!m_window->isValid()) {
            m_window.reset(); // El motivo ya está en el log
            return false;
        }
        InputManager::init(m_window->getSDLWindow());
        return true;
    }

    void PlatformModule::onFrameBegin(Application& app) {
        InputManager::update();

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) app.requestStop();
            if (m_eventCallback) m_eventCallback(event);
            InputManager::processEvent(event);
        }
    }

    void PlatformModule::onFrameEnd(Application& /*app*/) {
        m_window->swapBuffers();
    }

    void PlatformModule::onShutdown(Application& /*app*/) {
        InputManager::init(nullptr);
        m_window.reset();
    }
}
