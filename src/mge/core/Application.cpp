#include <mge/core/Application.h>
#include <mge/Version.h>
#include <mge/core/Log.h>
#include <mge/core/ecs/Camera.h>

#include <algorithm>

#include <cstdlib>
#include <string>

namespace mge {

    Application::Application(ApplicationConfig config) : m_config(std::move(config)) {
        m_viewport = {static_cast<unsigned>(std::max(m_config.width, 0)),
                      static_cast<unsigned>(std::max(m_config.height, 0))};
        // Frame cap from environment (CI / smoke tests), only if the game did not set one
        if (m_config.maxFrames == 0) {
            if (const char* env = std::getenv("MGE_MAX_FRAMES")) {
                char* end = nullptr;
                unsigned long long value = std::strtoull(env, &end, 10);
                if (end != env && *end == '\0') {
                    m_config.maxFrames = value;
                    MGE_LOG_INFO("App", "MGE_MAX_FRAMES=", value, ": application will exit after ", value,
                                 " frames");
                } else {
                    MGE_LOG_WARN("App", "Invalid MGE_MAX_FRAMES ('", env, "'); ignored");
                }
            }
        }
        // Vsync from environment (benchmarks: MGE_VSYNC=0 measures FPS without the monitor cap)
        if (const char* env = std::getenv("MGE_VSYNC")) {
            const std::string value = env;
            if (value == "0" || value == "1") {
                m_config.vsync = value == "1";
                MGE_LOG_INFO("App", "MGE_VSYNC=", value);
            } else {
                MGE_LOG_WARN("App", "Invalid MGE_VSYNC ('", env, "'): use 0 or 1; ignored");
            }
        }
        // Profiling from environment (MGE_PROFILE=0 disables the profiler at runtime)
        if (const char* env = std::getenv("MGE_PROFILE")) {
            const std::string value = env;
            if (value == "0") {
                m_config.profiling = false;
                MGE_LOG_INFO("App", "MGE_PROFILE=0: profiler disabled");
            } else if (value == "1") {
                m_config.profiling = true;
                MGE_LOG_INFO("App", "MGE_PROFILE=1: profiler enabled");
            } else {
                MGE_LOG_WARN("App", "Invalid MGE_PROFILE ('", env, "'): use 0 or 1; ignored");
            }
        }
        m_profiler.setEnabled(m_config.profiling);
        m_world.setProfiler(&m_profiler);
    }

    Application::~Application() = default;

    JobSystem& Application::jobs() {
        if (!m_jobs) {
            m_jobs = std::make_unique<JobSystem>();
            MGE_LOG_INFO("App", "Job pool: ", m_jobs->threadCount(), " threads");
        }
        return *m_jobs;
    }

    bool Application::initModules() {
        for (const auto& module : m_modules) {
            MGE_LOG_DEBUG("App", "Initializing module ", module->name());
            if (!module->onInit(*this)) {
                MGE_LOG_FATAL("App", "Failed to initialize module ", module->name());
                return false;
            }
            m_initializedModules++;
        }
        return true;
    }

    void Application::shutdownModules() {
        // Reverse order, and only those that initialized successfully
        while (m_initializedModules > 0) {
            m_initializedModules--;
            Module& module = *m_modules[m_initializedModules];
            MGE_LOG_DEBUG("App", "Shutting down module ", module.name());
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

        // Fixed steps from the accumulated scaled time. All in double, with a 1 ns tolerance:
        // summing 4 × (1/240) in floating point can be slightly less than 1/60 and miss a step.
        constexpr double kEpsilon = 1e-9;
        const double fixedDelta = m_fixedDeltaSeconds;
        m_time.m_accumulator += scaled;
        int steps = 0;
        {
            MGE_PROFILE_SCOPE(m_profiler, "fixed");
            while (m_time.m_accumulator + kEpsilon >= fixedDelta && steps < m_config.maxFixedStepsPerFrame) {
                m_world.run(SystemPhase::Fixed, m_time.m_fixedDelta);
                onFixedUpdate(m_time.m_fixedDelta);
                m_time.m_accumulator -= fixedDelta;
                m_time.m_fixedStepCount++;
                steps++;
            }
        }
        // Hit the cap: discard the whole backlog (leaving only the current fractional step)
        if (m_time.m_accumulator + kEpsilon >= fixedDelta) {
            MGE_LOG_DEBUG("App", "Slow frame: dropping ",
                          static_cast<int>(m_time.m_accumulator / fixedDelta), " fixed steps");
            while (m_time.m_accumulator + kEpsilon >= fixedDelta) m_time.m_accumulator -= fixedDelta;
        }
        if (m_time.m_accumulator < 0.0) m_time.m_accumulator = 0.0;
        m_time.m_alpha = static_cast<float>(m_time.m_accumulator / fixedDelta);
    }

    int Application::run() {
        MGE_LOG_INFO("App", "miniGameEngine v", kVersionString);
        // "Running" from the start: this way a requestStop() in onInit (or in a module) is respected
        // and the loop never runs (previously m_running = true was overwritten after onInit).
        m_running = true;
        if (!initModules()) {
            shutdownModules();
            return 1;
        }
        if (!onInit()) {
            MGE_LOG_FATAL("App", "Game failed to initialize; exiting");
            shutdownModules();
            return 1;
        }

        if (!m_clock) m_clock = std::make_unique<SteadyClock>();
        m_fixedDeltaSeconds = 1.0 / (m_config.tickRate > 0.0 ? m_config.tickRate : 60.0);
        m_time.m_fixedDelta = static_cast<float>(m_fixedDeltaSeconds);

        double lastTime = m_clock->now();

        while (m_running) {
            m_profiler.beginFrame();
            bool frameAborted = false;
            {
                MGE_PROFILE_SCOPE(m_profiler, "frame");

                {
                    MGE_PROFILE_SCOPE(m_profiler, "modules.begin");
                    for (const auto& module : m_modules) module->onFrameBegin(*this);
                }
                // A module may have requested stop (e.g., the window was closed)
                if (!m_running) {
                    frameAborted = true;
                } else {
                    const double now = m_clock->now();
                    advanceTime(now - lastTime); // onFixedUpdate 0..N times
                    lastTime = now;

                    {
                        MGE_PROFILE_SCOPE(m_profiler, "update");
                        m_world.run(SystemPhase::Update, m_time.m_delta);
                        onUpdate(m_time.m_delta);
                    }

                    CameraSystem::update(m_world.registry(), m_viewport);

                    {
                        MGE_PROFILE_SCOPE(m_profiler, "render");
                        m_world.run(SystemPhase::Render, m_time.m_delta);
                        onRender();
                    }

                    {
                        MGE_PROFILE_SCOPE(m_profiler, "modules.end");
                        for (const auto& module : m_modules) module->onFrameEnd(*this);
                    }
                }
            }
            m_profiler.endFrame();
            if (frameAborted) break;

            m_time.m_frameCount++;
            if (m_config.maxFrames > 0 && m_time.m_frameCount >= m_config.maxFrames) m_running = false;
        }

        // Running jobs may reference game data: finish them before the game frees anything
        if (m_jobs) m_jobs->waitIdle();
        onShutdown();
        shutdownModules();
        return m_exitCode;
    }
}
