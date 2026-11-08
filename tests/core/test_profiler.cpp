#include <doctest/doctest.h>

#include <mge/core/Application.h>
#include <mge/core/Clock.h>
#include <mge/core/Profiler.h>
#include <mge/core/ecs/World.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <thread>
#include <vector>

using namespace mge;

TEST_CASE("Profiler: scopes anidados tienen depth 0/1 y tiempo padre >= tiempo hijo") {
    Profiler profiler;
    profiler.beginFrame();
    {
        MGE_PROFILE_SCOPE(profiler, "parent");
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        {
            MGE_PROFILE_SCOPE(profiler, "child");
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }
    profiler.endFrame();

    const auto& entries = profiler.entries();
    REQUIRE(entries.size() == 2);
    CHECK(entries[0].name == "parent");
    CHECK(entries[0].depth == 0);
    CHECK(entries[1].name == "child");
    CHECK(entries[1].depth == 1);
    CHECK(entries[0].lastMs >= entries[1].lastMs);
}

TEST_CASE("Profiler: contador de llamadas (calls) por frame") {
    Profiler profiler;
    profiler.beginFrame();
    for (int i = 0; i < 5; ++i) {
        MGE_PROFILE_SCOPE(profiler, "work");
    }
    profiler.endFrame();

    const auto& entries = profiler.entries();
    REQUIRE(entries.size() == 1);
    CHECK(entries[0].name == "work");
    CHECK(entries[0].calls == 5);

    // Siguiente frame con 2 llamadas
    profiler.beginFrame();
    for (int i = 0; i < 2; ++i) {
        MGE_PROFILE_SCOPE(profiler, "work");
    }
    profiler.endFrame();

    CHECK(entries[0].calls == 2);
}

TEST_CASE("Profiler: orden estable de entradas entre frames") {
    Profiler profiler;
    profiler.beginFrame();
    { MGE_PROFILE_SCOPE(profiler, "first"); }
    { MGE_PROFILE_SCOPE(profiler, "second"); }
    { MGE_PROFILE_SCOPE(profiler, "third"); }
    profiler.endFrame();

    REQUIRE(profiler.entries().size() == 3);
    CHECK(profiler.entries()[0].name == "first");
    CHECK(profiler.entries()[1].name == "second");
    CHECK(profiler.entries()[2].name == "third");

    // Frame 2: se ejecutan en distinto orden
    profiler.beginFrame();
    { MGE_PROFILE_SCOPE(profiler, "third"); }
    { MGE_PROFILE_SCOPE(profiler, "first"); }
    profiler.endFrame();

    REQUIRE(profiler.entries().size() == 3);
    CHECK(profiler.entries()[0].name == "first");
    CHECK(profiler.entries()[1].name == "second");
    CHECK(profiler.entries()[2].name == "third");
}

TEST_CASE("Profiler: desactivado no registra entradas ni mediciones") {
    Profiler profiler;
    profiler.setEnabled(false);
    CHECK_FALSE(profiler.enabled());

    profiler.beginFrame();
    { MGE_PROFILE_SCOPE(profiler, "work"); }
    profiler.endFrame();

    CHECK(profiler.entries().empty());
}

TEST_CASE("Profiler: averageMs converge hacia lastMs") {
    Profiler profiler;
    std::vector<float> lastMsValues;
    lastMsValues.reserve(30);
    for (int frame = 0; frame < 30; ++frame) {
        profiler.beginFrame();
        profiler.begin("task");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        profiler.end();
        profiler.endFrame();

        const auto& entries = profiler.entries();
        REQUIRE(entries.size() == 1);
        lastMsValues.push_back(entries[0].lastMs);
    }
    const auto& e = profiler.entries();
    REQUIRE(e.size() == 1);
    CHECK(e[0].averageMs > 0.0f);
    const auto [minIt, maxIt] = std::minmax_element(lastMsValues.begin(), lastMsValues.end());
    CHECK(e[0].averageMs >= *minIt);
    CHECK(e[0].averageMs <= *maxIt);
}

TEST_CASE("Profiler: Application con World y dos sistemas produce entradas con los nombres de los sistemas") {
    struct TestProfilerApp : public Application {
        explicit TestProfilerApp(ApplicationConfig cfg) : Application(std::move(cfg)) {}
    };

    ApplicationConfig config;
    config.headless = true;
    config.maxFrames = 3;
    TestProfilerApp app(config);
    app.setClock(std::make_unique<ManualClock>(1.0 / 60.0));

    bool sysARan = false;
    bool sysBRan = false;

    app.world().addSystem(SystemPhase::Update, "SystemAlpha", [&](World&, float) { sysARan = true; });
    app.world().addSystem(SystemPhase::Update, "SystemBeta", [&](World&, float) { sysBRan = true; });

    const int exitCode = app.run();
    CHECK(exitCode == 0);
    CHECK(sysARan);
    CHECK(sysBRan);

    const auto& entries = app.profiler().entries();
    bool foundAlpha = false;
    bool foundBeta = false;
    bool foundFrame = false;
    bool foundUpdate = false;

    for (const auto& entry : entries) {
        if (entry.name == "SystemAlpha") {
            foundAlpha = true;
            CHECK(entry.calls == 1);
        }
        if (entry.name == "SystemBeta") {
            foundBeta = true;
            CHECK(entry.calls == 1);
        }
        if (entry.name == "frame") {
            foundFrame = true;
        }
        if (entry.name == "update") {
            foundUpdate = true;
        }
    }
    CHECK(foundAlpha);
    CHECK(foundBeta);
    CHECK(foundFrame);
    CHECK(foundUpdate);
}
