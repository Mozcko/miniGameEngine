#pragma once

#include <mge/core/Clock.h>
#include <mge/core/Jobs.h>
#include <mge/core/Module.h>
#include <mge/core/Profiler.h>
#include <mge/core/Time.h>
#include <mge/core/ecs/World.h>

#include <glm/glm.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace mge {

    struct ApplicationConfig {
        std::string title = "miniGameEngine";
        int width = 1280;
        int height = 720;
        // Hint for the game: no window/GPU (tests, servers, CI). The core never creates a window;
        // a headless game simply omits PlatformModule.
        bool headless = false;
        // 0 = unlimited (until requestStop). Useful for tests and headless runs.
        std::uint64_t maxFrames = 0;
        // Fixed timestep: onFixedUpdate runs tickRate times per second (of scaled time)
        double tickRate = 60.0;
        // Maximum fixed steps per frame: if a frame takes too long, excess time is discarded
        // (avoids the "spiral of death", where each frame accumulates more work than the previous one)
        int maxFixedStepsPerFrame = 5;
        // A single frame never counts for more than this (OS pauses, breakpoints, dragging the window)
        double maxFrameTime = 0.25;
        // Synchronize presentation with the display (no tearing; caps FPS to the display refresh rate).
        // false = as fast as possible. Applied by platform/GPU if present; unused by the core.
        bool vsync = true;
        // Per-frame timing instrumentation (MGE_PROFILE=0 in the environment disables it)
        bool profiling = true;
    };

    // Engine runner: owns the loop and the modules. Lives in the core (no SDL or GPU).
    // A game inherits from Application, adds the modules it needs, and defines createApplication().
    class Application {
    public:
        explicit Application(ApplicationConfig config = {});
        virtual ~Application();

        Application(const Application&) = delete;
        Application& operator=(const Application&) = delete;

        // Initializes modules and game, runs the loop, and shuts everything down. Returns the exit code:
        // 1 if initialization failed (the reason is in the log); otherwise the value from setExitCode (0 by
        // default). The MGE_MAX_FRAMES environment variable (if config.maxFrames is 0) caps the frame count:
        // useful for running any game for a few frames in CI.
        int run();

        // Exit code returned by run() on normal termination (e.g., a failing headless test)
        void setExitCode(int code) { m_exitCode = code; }

        // Stops the loop at the end of the current frame
        void requestStop() { m_running = false; }
        bool isRunning() const { return m_running; }

        const ApplicationConfig& getConfig() const { return m_config; }
        std::uint64_t getFrameCount() const { return m_time.frameCount(); }

        // Time state (delta, fixed step, alpha, scale). time().setTimeScale(0) = pause.
        Time& time() { return m_time; }
        const Time& time() const { return m_time; }

        // Frame profiler (CPU). Measures phase and system timings.
        Profiler& profiler() { return m_profiler; }
        const Profiler& profiler() const { return m_profiler; }

        // Entities + systems. The loop runs the phases: Fixed on each fixed step (before
        // onFixedUpdate), Update before onUpdate, and Render before onRender.
        World& world() { return m_world; }
        const World& world() const { return m_world; }

        // Size in pixels of the image being drawn. Updated each frame by the GPU module (the
        // swapchain follows the window); without a GPU it is config.width × config.height. Before the Render
        // phase, the loop adjusts autoAspect cameras to this size (CameraSystem).
        const glm::uvec2& viewport() const { return m_viewport; }
        void setViewport(const glm::uvec2& viewport) { m_viewport = viewport; }

        // Engine thread pool (created on first request). For heavy, independent work:
        // generating/meshing chunks, etc. Destroyed with the Application, after onShutdown.
        JobSystem& jobs();

        // Replaces the clock (before run()). Defaults to SteadyClock (real time).
        // Deterministic tests/headless runs: setClock(std::make_unique<ManualClock>(1.0 / 60.0)).
        void setClock(std::unique_ptr<Clock> clock) { m_clock = std::move(clock); }

        // Adds a module (before run()). Returns a reference to configure it.
        template <typename T, typename... Args> T& addModule(Args&&... args) {
            static_assert(std::is_base_of_v<Module, T>, "T must inherit from mge::Module");
            auto module = std::make_unique<T>(std::forward<Args>(args)...);
            T& ref = *module;
            m_modules.push_back(std::move(module));
            return ref;
        }

        // Looks up a module by type (nullptr if not present)
        template <typename T> T* getModule() const {
            for (const auto& module : m_modules) {
                if (auto* typed = dynamic_cast<T*>(module.get())) return typed;
            }
            return nullptr;
        }

    protected:
        // Game hooks. onInit runs after initializing the modules (e.g., with the GPU ready).
        // Returning false aborts startup.
        virtual bool onInit() { return true; }
        // Deterministic simulation: 0..N times per frame, always with the same fixedDt (1 / tickRate)
        virtual void onFixedUpdate(float /*fixedDt*/) {}
        // Once per frame with the scaled delta (0 when paused). Camera, input, animations...
        virtual void onUpdate(float /*dt*/) {}
        // Once per frame. time().alpha() = fraction of the next fixed step (for interpolation)
        virtual void onRender() {}
        // Before shutting down the modules (the game's GPU resources are still valid)
        virtual void onShutdown() {}

    private:
        bool initModules();
        void shutdownModules();
        // Advances time with the real frame delta and runs the due fixed steps
        void advanceTime(double rawFrameTime);

        ApplicationConfig m_config;
        std::vector<std::unique_ptr<Module>> m_modules;
        std::size_t m_initializedModules = 0; // The first N initialized successfully
        bool m_running = false;
        int m_exitCode = 0;
        Time m_time;
        Profiler m_profiler;
        World m_world;
        std::unique_ptr<JobSystem> m_jobs;
        glm::uvec2 m_viewport{0, 0};
        double m_fixedDeltaSeconds = 1.0 / 60.0; // Full precision for the accumulator
        std::unique_ptr<Clock> m_clock;
    };

    // Defined by the game: returns its Application. Called by the engine's main() (mge_main).
    Application* createApplication();
}
