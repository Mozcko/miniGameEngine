#include <mge/core/Application.h>
#include <mge/core/Log.h>

#include <chrono>
#include <cstdlib>
#include <string>

namespace mge {

    Application::Application(ApplicationConfig config) : m_config(std::move(config)) {
        // Límite de frames desde el entorno (CI / smoke tests), solo si el juego no fijó uno
        if (m_config.maxFrames == 0) {
            if (const char* env = std::getenv("MGE_MAX_FRAMES")) {
                char* end = nullptr;
                unsigned long long value = std::strtoull(env, &end, 10);
                if (end != env && *end == '\0') {
                    m_config.maxFrames = value;
                    MGE_LOG_INFO("App", "MGE_MAX_FRAMES=", value, ": la aplicación saldrá tras ", value,
                                 " frames");
                } else {
                    MGE_LOG_WARN("App", "MGE_MAX_FRAMES inválido ('", env, "'); se ignora");
                }
            }
        }
    }

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
        return m_exitCode;
    }
}
