#include <mge/platform/PlatformModule.h>

#include <mge/core/Application.h>
#include <mge/core/Log.h>
#include <mge/platform/InputManager.h>

namespace mge {

    namespace {
        // Cada módulo libera solo su subsistema; el último en salir cierra SDL por completo.
        // (SDL lleva la cuenta de referencias de cada subsistema.)
        void quitSdlSubsystem(SDL_InitFlags flags) {
            SDL_QuitSubSystem(flags);
            if (SDL_WasInit(0) == 0) SDL_Quit();
        }
    }

    bool PlatformModule::onInit(Application& app) {
        if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
            MGE_LOG_FATAL("Platform", "No se pudo inicializar el video de SDL: ", SDL_GetError());
            return false;
        }

        const ApplicationConfig& config = app.getConfig();
        m_window = std::make_unique<Window>(config.title, config.width, config.height);
        if (!m_window->isValid()) {
            m_window.reset(); // El motivo ya está en el log
            quitSdlSubsystem(SDL_INIT_VIDEO);
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
        // Primero la ventana y el contexto OpenGL, después el subsistema de video
        m_window.reset();
        quitSdlSubsystem(SDL_INIT_VIDEO);
    }
}
