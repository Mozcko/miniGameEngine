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

TEST_CASE("JobSystem: each job runs once and futures return the result") {
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

TEST_CASE("JobSystem: waitIdle waits for all jobs to finish") {
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

TEST_CASE("JobSystem: exceptions propagate through the future") {
    JobSystem jobs(1);
    auto failing = jobs.submit([]() -> int { throw std::runtime_error("failure"); });
    CHECK_THROWS_AS(failing.get(), std::runtime_error);
    CHECK(jobs.submit([] { return 7; }).get() == 7); // Thread stays alive
}

TEST_CASE("JobSystem: destroying with pending jobs is clean (unstarted jobs are dropped)") {
    std::future<int> notStarted;
    std::atomic<bool> release{false};
    {
        JobSystem jobs(1);
        jobs.submit([&release] {
            while (!release) std::this_thread::yield();
        });
        notStarted = jobs.submit([] { return 1; }); // Behind the job blocking the single thread
        release = true;
    } // ~JobSystem: finishes the first, drops or finishes the second
    // If dropped: broken_promise; if it managed to run: 1. Never hangs or leaks memory
    // (get() outside CHECK: doctest would catch the exception itself)
    bool ranOrDropped = false;
    try {
        ranOrDropped = notStarted.get() == 1;
    } catch (const std::future_error& error) {
        ranOrDropped = error.code() == std::future_errc::broken_promise;
    }
    CHECK(ranOrDropped);
}

TEST_CASE("Application: jobs() is created lazily and waits for jobs before onShutdown") {
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
