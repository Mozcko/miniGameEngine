#include <doctest/doctest.h>
#include <mge/core/Application.h>
#include <mge/core/Jobs.h>

#include <atomic>
#include <chrono>
#include <future>
#include <numeric>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace mge;

TEST_CASE("JobSystem: cada trabajo corre una sola vez y los futures traen el resultado") {
    JobSystem jobs(4);
    CHECK(jobs.threadCount() == 4);
    std::vector<std::atomic<int>> runs(1000);
    std::vector<std::future<int>> results;
    for (int i = 0; i < 1000; i++)
        results.push_back(jobs.submit([i, &runs] {
            runs[static_cast<std::size_t>(i)]++;
            return i * 2;
        }));
    long sum = 0;
    for (auto& result : results) sum += result.get();
    CHECK(sum == 2L * (999L * 1000L / 2L));
    for (const auto& count : runs) CHECK(count.load() == 1);
}

TEST_CASE("JobSystem: waitIdle espera a que terminen todos") {
    JobSystem jobs(3);
    std::atomic<int> done{0};
    for (int i = 0; i < 50; i++)
        jobs.submit([&done] {
            std::this_thread::sleep_for(std::chrono::microseconds(200));
            done++;
        });
    jobs.waitIdle();
    CHECK(done.load() == 50);
    CHECK(jobs.pendingCount() == 0);
}

TEST_CASE("JobSystem: las excepciones viajan en el future") {
    JobSystem jobs(1);
    auto failing = jobs.submit([]() -> int { throw std::runtime_error("falla"); });
    CHECK_THROWS_AS(failing.get(), std::runtime_error);
    CHECK(jobs.submit([] { return 7; }).get() == 7); // El hilo sigue vivo
}

TEST_CASE("JobSystem: destruir con trabajos pendientes es limpio (los no empezados se descartan)") {
    std::future<int> notStarted;
    std::atomic<bool> release{false};
    {
        JobSystem jobs(1);
        jobs.submit([&release] {
            while (!release) std::this_thread::yield();
        });
        notStarted = jobs.submit([] { return 1; }); // Detrás del que bloquea el único hilo
        release = true;
    } // ~JobSystem: termina el primero, descarta o termina el segundo
    // Si se descartó: broken_promise; si alcanzó a correr: 1. Nunca cuelga ni pierde memoria
    // (get() fuera del CHECK: doctest atraparía la excepción por su cuenta)
    bool ranOrDropped = false;
    try {
        ranOrDropped = notStarted.get() == 1;
    } catch (const std::future_error& error) {
        ranOrDropped = error.code() == std::future_errc::broken_promise;
    }
    CHECK(ranOrDropped);
}

TEST_CASE("Application: jobs() se crea al pedirlo y espera los trabajos antes de onShutdown") {
    struct App : Application {
        explicit App(ApplicationConfig config) : Application(std::move(config)) {}
        std::atomic<bool> finished{false};
        bool finishedAtShutdown = false;
        bool onInit() override {
            jobs().submit([this] {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                finished = true;
            });
            return true;
        }
        void onShutdown() override { finishedAtShutdown = finished; }
    };
    ApplicationConfig config;
    config.maxFrames = 1;
    App app(config);
    CHECK(app.run() == 0);
    CHECK(app.finishedAtShutdown);
    CHECK(app.jobs().threadCount() >= 1);
}
