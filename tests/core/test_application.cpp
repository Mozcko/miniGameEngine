#include <doctest/doctest.h>
#include <mge/core/Application.h>
#include <mge/core/Log.h>

#include <cstdlib>
#include <string>
#include <vector>

using namespace mge;

namespace {
    // Shared call log to verify execution order
    using CallLog = std::vector<std::string>;

    class RecordingModule : public Module {
    public:
        RecordingModule(std::string moduleName, CallLog& log, bool initOk = true)
            : m_name(std::move(moduleName)), m_log(log), m_initOk(initOk) {}

        const char* name() const override { return m_name.c_str(); }
        bool onInit(Application&) override {
            m_log.push_back(m_name + ".init");
            return m_initOk;
        }
        void onFrameBegin(Application&) override { m_log.push_back(m_name + ".begin"); }
        void onFrameEnd(Application&) override { m_log.push_back(m_name + ".end"); }
        void onShutdown(Application&) override { m_log.push_back(m_name + ".shutdown"); }

    private:
        std::string m_name;
        CallLog& m_log;
        bool m_initOk;
    };

    // Module that requests stop on its N-th frame (like a closing window)
    class QuitModule : public Module {
    public:
        explicit QuitModule(std::uint64_t quitOnFrame) : m_quitOnFrame(quitOnFrame) {}
        const char* name() const override { return "Quit"; }
        void onFrameBegin(Application& app) override {
            if (++m_frames == m_quitOnFrame) app.requestStop();
        }

    private:
        std::uint64_t m_quitOnFrame;
        std::uint64_t m_frames = 0;
    };

    class TestApp : public Application {
    public:
        TestApp(ApplicationConfig config, CallLog& log, bool initOk = true)
            : Application(std::move(config)), m_log(log), m_initOk(initOk) {}

        int updates = 0;
        int renders = 0;
        int stopAfterUpdates = 0; // 0 = do not request stop

    protected:
        bool onInit() override {
            m_log.push_back("game.init");
            return m_initOk;
        }
        void onUpdate(float dt) override {
            CHECK(dt >= 0.0f);
            updates++;
            m_log.push_back("game.update");
            if (stopAfterUpdates > 0 && updates == stopAfterUpdates) requestStop();
        }
        void onRender() override {
            renders++;
            m_log.push_back("game.render");
        }
        void onShutdown() override { m_log.push_back("game.shutdown"); }

    private:
        CallLog& m_log;
        bool m_initOk;
    };

    ApplicationConfig headlessConfig(std::uint64_t maxFrames) {
        ApplicationConfig config;
        config.headless = true;
        config.maxFrames = maxFrames;
        return config;
    }

    // Silences expected error logs during a test
    struct QuietLog {
        QuietLog() {
            Log::setSink([](const LogRecord&) {});
        }
        ~QuietLog() { Log::setSink(nullptr); }
    };
}

TEST_CASE("A headless app without modules runs exactly maxFrames frames") {
    CallLog log;
    TestApp app(headlessConfig(10), log);
    CHECK(app.run() == 0);
    CHECK(app.updates == 10);
    CHECK(app.renders == 10);
    CHECK(app.getFrameCount() == 10);
    CHECK_FALSE(app.isRunning());
}

TEST_CASE("Order: modules initialize in order, game next, and everything shuts down in reverse") {
    CallLog log;
    TestApp app(headlessConfig(1), log);
    app.addModule<RecordingModule>("A", log);
    app.addModule<RecordingModule>("B", log);
    CHECK(app.run() == 0);

    CallLog expected = {"A.init",      "B.init", "game.init", "A.begin",       "B.begin",    "game.update",
                        "game.render", "A.end",  "B.end",     "game.shutdown", "B.shutdown", "A.shutdown"};
    CHECK(log == expected);
}

TEST_CASE("If a module fails to initialize, only previous ones shut down and the game does not start") {
    QuietLog quiet;
    CallLog log;
    TestApp app(headlessConfig(5), log);
    app.addModule<RecordingModule>("A", log);
    app.addModule<RecordingModule>("B", log, /*initOk=*/false);
    app.addModule<RecordingModule>("C", log);
    CHECK(app.run() == 1);

    CallLog expected = {"A.init", "B.init", "A.shutdown"};
    CHECK(log == expected);
    CHECK(app.updates == 0);
}

TEST_CASE("If game onInit fails, modules shut down and run() returns 1") {
    QuietLog quiet;
    CallLog log;
    TestApp app(headlessConfig(5), log, /*initOk=*/false);
    app.addModule<RecordingModule>("A", log);
    CHECK(app.run() == 1);

    CallLog expected = {"A.init", "game.init", "A.shutdown"};
    CHECK(log == expected);
}

TEST_CASE("requestStop from the game ends at the end of the current frame") {
    CallLog log;
    TestApp app(headlessConfig(0), log); // No frame limit
    app.stopAfterUpdates = 3;
    CHECK(app.run() == 0);
    CHECK(app.updates == 3);
    CHECK(app.renders == 3); // Current frame completes
}

TEST_CASE("A module requesting stop in onFrameBegin skips that frame's update") {
    CallLog log;
    TestApp app(headlessConfig(0), log);
    app.addModule<QuitModule>(4);
    CHECK(app.run() == 0);
    CHECK(app.updates == 3);
}

TEST_CASE("getModule finds modules by type") {
    CallLog log;
    TestApp app(headlessConfig(1), log);
    auto& recording = app.addModule<RecordingModule>("A", log);
    CHECK(app.getModule<RecordingModule>() == &recording);
    CHECK(app.getModule<QuitModule>() == nullptr);
}

TEST_CASE("setExitCode sets the return value of run()") {
    CallLog log;
    TestApp app(headlessConfig(1), log);
    app.setExitCode(3);
    CHECK(app.run() == 3);
}

namespace {
    // Sets/unsets an environment variable during a test (POSIX)
    struct ScopedEnv {
        std::string name;
        ScopedEnv(const char* n, const char* value) : name(n) { setenv(n, value, 1); }
        ~ScopedEnv() { unsetenv(name.c_str()); }
    };
}

TEST_CASE("MGE_MAX_FRAMES limits frames if config does not set maxFrames") {
    CallLog log;
    ScopedEnv env("MGE_MAX_FRAMES", "7");
    TestApp app(headlessConfig(0), log);
    CHECK(app.run() == 0);
    CHECK(app.updates == 7);
}

TEST_CASE("config maxFrames takes priority over MGE_MAX_FRAMES") {
    CallLog log;
    ScopedEnv env("MGE_MAX_FRAMES", "7");
    TestApp app(headlessConfig(2), log);
    CHECK(app.run() == 0);
    CHECK(app.updates == 2);
}

TEST_CASE("An invalid MGE_MAX_FRAMES is ignored") {
    QuietLog quiet;
    CallLog log;
    ScopedEnv env("MGE_MAX_FRAMES", "abc");
    TestApp app(headlessConfig(0), log);
    app.stopAfterUpdates = 4;
    CHECK(app.run() == 0);
    CHECK(app.updates == 4);
}

TEST_CASE("requestStop inside onInit skips the loop (regression: previously ignored)") {
    class StopInInit : public Application {
    public:
        StopInInit() : Application(ApplicationConfig{}) {} // No maxFrames: if ignored, it would never end
        int updates = 0;
        bool shutdownCalled = false;

    protected:
        bool onInit() override {
            requestStop();
            return true;
        }
        void onUpdate(float) override {
            if (++updates > 3) requestStop(); // Safety net so the test never hangs
        }
        void onShutdown() override { shutdownCalled = true; }
    };
    StopInInit app;
    CHECK(app.run() == 0);
    CHECK(app.updates == 0);
    CHECK(app.shutdownCalled);
}

TEST_CASE("MGE_VSYNC from environment overrides ApplicationConfig::vsync") {
    {
        ScopedEnv env("MGE_VSYNC", "0");
        CHECK_FALSE(Application(ApplicationConfig{}).getConfig().vsync);
    }
    {
        ScopedEnv env("MGE_VSYNC", "1");
        ApplicationConfig off;
        off.vsync = false;
        CHECK(Application(off).getConfig().vsync);
    }
    ScopedEnv env("MGE_VSYNC", "maybe"); // Invalid: ignored
    CHECK(Application(ApplicationConfig{}).getConfig().vsync);
}
