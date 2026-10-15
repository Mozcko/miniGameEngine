#include <doctest/doctest.h>
#include <mge/core/Application.h>
#include <mge/core/Log.h>

#include <cstdlib>
#include <string>
#include <vector>

using namespace mge;

namespace {
    // Registro compartido de llamadas, para verificar el orden
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

    // Módulo que pide salir en su N-ésimo frame (como una ventana que se cierra)
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
        int stopAfterUpdates = 0; // 0 = no pedir salir

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

    // Silencia el log de errores esperados durante un test
    struct QuietLog {
        QuietLog() {
            Log::setSink([](const LogRecord&) {});
        }
        ~QuietLog() { Log::setSink(nullptr); }
    };
}

TEST_CASE("Una app headless sin módulos corre exactamente maxFrames frames") {
    CallLog log;
    TestApp app(headlessConfig(10), log);
    CHECK(app.run() == 0);
    CHECK(app.updates == 10);
    CHECK(app.renders == 10);
    CHECK(app.getFrameCount() == 10);
    CHECK_FALSE(app.isRunning());
}

TEST_CASE("Orden: módulos se inicializan en orden, el juego después, y todo se cierra al revés") {
    CallLog log;
    TestApp app(headlessConfig(1), log);
    app.addModule<RecordingModule>("A", log);
    app.addModule<RecordingModule>("B", log);
    CHECK(app.run() == 0);

    CallLog expected = {"A.init",      "B.init", "game.init", "A.begin",       "B.begin",    "game.update",
                        "game.render", "A.end",  "B.end",     "game.shutdown", "B.shutdown", "A.shutdown"};
    CHECK(log == expected);
}

TEST_CASE("Si un módulo falla al iniciar, solo se cierran los anteriores y el juego no arranca") {
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

TEST_CASE("Si onInit del juego falla, se cierran los módulos y run() devuelve 1") {
    QuietLog quiet;
    CallLog log;
    TestApp app(headlessConfig(5), log, /*initOk=*/false);
    app.addModule<RecordingModule>("A", log);
    CHECK(app.run() == 1);

    CallLog expected = {"A.init", "game.init", "A.shutdown"};
    CHECK(log == expected);
}

TEST_CASE("requestStop desde el juego termina al final del frame actual") {
    CallLog log;
    TestApp app(headlessConfig(0), log); // Sin límite de frames
    app.stopAfterUpdates = 3;
    CHECK(app.run() == 0);
    CHECK(app.updates == 3);
    CHECK(app.renders == 3); // El frame en curso se completa
}

TEST_CASE("Un módulo que pide salir en onFrameBegin evita el update de ese frame") {
    CallLog log;
    TestApp app(headlessConfig(0), log);
    app.addModule<QuitModule>(4);
    CHECK(app.run() == 0);
    CHECK(app.updates == 3);
}

TEST_CASE("getModule encuentra módulos por tipo") {
    CallLog log;
    TestApp app(headlessConfig(1), log);
    auto& recording = app.addModule<RecordingModule>("A", log);
    CHECK(app.getModule<RecordingModule>() == &recording);
    CHECK(app.getModule<QuitModule>() == nullptr);
}

TEST_CASE("setExitCode define lo que devuelve run()") {
    CallLog log;
    TestApp app(headlessConfig(1), log);
    app.setExitCode(3);
    CHECK(app.run() == 3);
}

namespace {
    // Fija/borra una variable de entorno durante un test (POSIX)
    struct ScopedEnv {
        std::string name;
        ScopedEnv(const char* n, const char* value) : name(n) { setenv(n, value, 1); }
        ~ScopedEnv() { unsetenv(name.c_str()); }
    };
}

TEST_CASE("MGE_MAX_FRAMES limita los frames si el config no fija maxFrames") {
    CallLog log;
    ScopedEnv env("MGE_MAX_FRAMES", "7");
    TestApp app(headlessConfig(0), log);
    CHECK(app.run() == 0);
    CHECK(app.updates == 7);
}

TEST_CASE("maxFrames del config tiene prioridad sobre MGE_MAX_FRAMES") {
    CallLog log;
    ScopedEnv env("MGE_MAX_FRAMES", "7");
    TestApp app(headlessConfig(2), log);
    CHECK(app.run() == 0);
    CHECK(app.updates == 2);
}

TEST_CASE("Un MGE_MAX_FRAMES inválido se ignora") {
    QuietLog quiet;
    CallLog log;
    ScopedEnv env("MGE_MAX_FRAMES", "abc");
    TestApp app(headlessConfig(0), log);
    app.stopAfterUpdates = 4;
    CHECK(app.run() == 0);
    CHECK(app.updates == 4);
}

TEST_CASE("requestStop dentro de onInit evita el loop (regresión: antes se ignoraba)") {
    class StopInInit : public Application {
    public:
        StopInInit() : Application(ApplicationConfig{}) {} // Sin maxFrames: si se ignorara, no terminaría
        int updates = 0;
        bool shutdownCalled = false;

    protected:
        bool onInit() override {
            requestStop();
            return true;
        }
        void onUpdate(float) override {
            if (++updates > 3) requestStop(); // Red de seguridad para que el test no se cuelgue
        }
        void onShutdown() override { shutdownCalled = true; }
    };
    StopInInit app;
    CHECK(app.run() == 0);
    CHECK(app.updates == 0);
    CHECK(app.shutdownCalled);
}

TEST_CASE("MGE_VSYNC del entorno pisa ApplicationConfig::vsync") {
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
    ScopedEnv env("MGE_VSYNC", "tal vez"); // Inválido: se ignora
    CHECK(Application(ApplicationConfig{}).getConfig().vsync);
}
