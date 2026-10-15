#include <mge/core/Application.h>
#include <mge/core/Log.h>

#include <chrono>

namespace mge {

    Application::Application(ApplicationConfig config) : m_config(std::move(config)) {}

    Application::~Application() = default;

    bool Application::initModules() {
        for (const auto& module : m_modules) {
            MGE_LOG_DEBUG("App", "Inicializando módulo ", module->name());
            if (!module->onInit(*this)) {
                MGE_LOG_FATAL("App", "Falló la inicialización del módulo ", module->name());
                return false;
            }
            m_initializedModules++;
        }
        return true;
    }

    void Application::shutdownModules() {
        // Orden inverso, y solo los que inicializaron bien
        while (m_initializedModules > 0) {
            m_initializedModules--;
            Module& module = *m_modules[m_initializedModules];
            MGE_LOG_DEBUG("App", "Cerrando módulo ", module.name());
            module.onShutdown(*this);
        }
    }

    int Application::run() {
        if (!initModules()) {
            shutdownModules();
            return 1;
        }
        if (!onInit()) {
            MGE_LOG_FATAL("App", "El juego no se pudo inicializar; saliendo");
            shutdownModules();
            return 1;
        }

        m_running = true;
        m_frameCount = 0;
        auto lastTime = std::chrono::steady_clock::now();

        while (m_running) {
            auto now = std::chrono::steady_clock::now();
            std::chrono::duration<float> elapsed = now - lastTime;
            lastTime = now;

            for (const auto& module : m_modules) module->onFrameBegin(*this);
            // Un módulo pudo pedir salir (ej: se cerró la ventana)
            if (!m_running) break;

            onUpdate(elapsed.count());
            onRender();

            for (const auto& module : m_modules) module->onFrameEnd(*this);

            m_frameCount++;
            if (m_config.maxFrames > 0 && m_frameCount >= m_config.maxFrames) m_running = false;
        }

        onShutdown();
        shutdownModules();
        return 0;
    }
}
