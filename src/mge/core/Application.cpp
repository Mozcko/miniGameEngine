#include <mge/core/Application.h>
#include <mge/core/Log.h>
#include <mge/core/ecs/Camera.h>

#include <algorithm>

#include <cstdlib>
#include <string>

namespace mge {

    Application::Application(ApplicationConfig config) : m_config(std::move(config)) {
        m_viewport = {static_cast<unsigned>(std::max(m_config.width, 0)),
                      static_cast<unsigned>(std::max(m_config.height, 0))};
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
        // Vsync desde el entorno (benchmarks: MGE_VSYNC=0 mide los FPS sin el límite del monitor)
        if (const char* env = std::getenv("MGE_VSYNC")) {
            const std::string value = env;
            if (value == "0" || value == "1") {
                m_config.vsync = value == "1";
                MGE_LOG_INFO("App", "MGE_VSYNC=", value);
            } else {
                MGE_LOG_WARN("App", "MGE_VSYNC inválido ('", env, "'): usar 0 o 1; se ignora");
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

    void Application::advanceTime(double rawFrameTime) {
        if (rawFrameTime < 0.0) rawFrameTime = 0.0;
        if (rawFrameTime > m_config.maxFrameTime) rawFrameTime = m_config.maxFrameTime;

        const double scaled = rawFrameTime * m_time.m_timeScale;
        m_time.m_unscaledDelta = static_cast<float>(rawFrameTime);
        m_time.m_delta = static_cast<float>(scaled);
        m_time.m_unscaledElapsed += rawFrameTime;
        m_time.m_elapsed += scaled;

        // Pasos fijos con el tiempo escalado acumulado. Todo en double, y con una tolerancia de 1 ns:
        // sumar 4 × (1/240) en coma flotante puede dar apenas menos que 1/60 y perder un paso.
        constexpr double kEpsilon = 1e-9;
        const double fixedDelta = m_fixedDeltaSeconds;
        m_time.m_accumulator += scaled;
        int steps = 0;
        while (m_time.m_accumulator + kEpsilon >= fixedDelta && steps < m_config.maxFixedStepsPerFrame) {
            m_world.run(SystemPhase::Fixed, m_time.m_fixedDelta);
            onFixedUpdate(m_time.m_fixedDelta);
            m_time.m_accumulator -= fixedDelta;
            m_time.m_fixedStepCount++;
            steps++;
        }
        // Se llegó al tope: descartar el atraso entero (queda solo la fracción del paso actual)
        if (m_time.m_accumulator + kEpsilon >= fixedDelta) {
            MGE_LOG_DEBUG("App", "Frame lento: se descartan ",
                          static_cast<int>(m_time.m_accumulator / fixedDelta), " pasos fijos");
            while (m_time.m_accumulator + kEpsilon >= fixedDelta) m_time.m_accumulator -= fixedDelta;
        }
        if (m_time.m_accumulator < 0.0) m_time.m_accumulator = 0.0;
        m_time.m_alpha = static_cast<float>(m_time.m_accumulator / fixedDelta);
    }

    int Application::run() {
        // "Corriendo" desde el principio: así un requestStop() en onInit (o en un módulo) se respeta
        // y el loop no llega a ejecutarse (antes se pisaba con m_running = true después de onInit).
        m_running = true;
        if (!initModules()) {
            shutdownModules();
            return 1;
        }
        if (!onInit()) {
            MGE_LOG_FATAL("App", "El juego no se pudo inicializar; saliendo");
            shutdownModules();
            return 1;
        }

        if (!m_clock) m_clock = std::make_unique<SteadyClock>();
        m_fixedDeltaSeconds = 1.0 / (m_config.tickRate > 0.0 ? m_config.tickRate : 60.0);
        m_time.m_fixedDelta = static_cast<float>(m_fixedDeltaSeconds);

        double lastTime = m_clock->now();

        while (m_running) {
            for (const auto& module : m_modules) module->onFrameBegin(*this);
            // Un módulo pudo pedir salir (ej: se cerró la ventana)
            if (!m_running) break;

            const double now = m_clock->now();
            advanceTime(now - lastTime); // onFixedUpdate 0..N veces
            lastTime = now;

            m_world.run(SystemPhase::Update, m_time.m_delta);
            onUpdate(m_time.m_delta);
            CameraSystem::update(m_world.registry(), m_viewport);
            m_world.run(SystemPhase::Render, m_time.m_delta);
            onRender();

            for (const auto& module : m_modules) module->onFrameEnd(*this);

            m_time.m_frameCount++;
            if (m_config.maxFrames > 0 && m_time.m_frameCount >= m_config.maxFrames) m_running = false;
        }

        onShutdown();
        shutdownModules();
        return m_exitCode;
    }
}
