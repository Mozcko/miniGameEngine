#include <doctest/doctest.h>
#include <mge/core/Coroutine.h>

#include <vector>

using namespace mge;

TEST_CASE("Coroutine executes actions, waitSeconds, and onComplete in order") {
    CoroutineRunner runner;
    std::vector<int> trace;

    Coroutine script;
    script.then([&] { trace.push_back(1); })
        .waitSeconds(0.5f)
        .then([&] { trace.push_back(2); })
        .waitSeconds(0.5f)
        .then([&] { trace.push_back(3); })
        .onComplete([&] { trace.push_back(99); });

    CoroutineHandle handle = runner.start(std::move(script));
    CHECK(handle.isValid());
    CHECK(runner.isRunning(handle));
    CHECK(runner.runningCount() == 1);

    runner.update(0.25f);
    CHECK(trace == std::vector<int>{1});

    runner.update(0.25f);
    CHECK(trace == std::vector<int>{1, 2});

    runner.update(0.5f);
    CHECK(trace == std::vector<int>{1, 2, 3, 99});
    CHECK_FALSE(runner.isRunning(handle));
    CHECK(runner.runningCount() == 0);
}

TEST_CASE("Coroutine waitFrames, waitUntil, waitWhile, and repeat") {
    CoroutineRunner runner;
    std::vector<int> trace;
    bool gateOpen = false;
    bool busy = true;

    Coroutine script;
    script.then([&] { trace.push_back(1); })
        .waitFrames(2)
        .then([&] { trace.push_back(2); })
        .waitUntil([&] { return gateOpen; })
        .then([&] { trace.push_back(3); })
        .waitWhile([&] { return busy; })
        .repeat(2, [&](Coroutine& sub) { sub.then([&] { trace.push_back(4); }); });

    runner.start(std::move(script));

    runner.update(0.016f); // runs 1, enters waitFrames(2)
    CHECK(trace == std::vector<int>{1});

    runner.update(0.016f); // frame 1 of 2
    CHECK(trace == std::vector<int>{1});

    runner.update(0.016f); // frame 2 of 2 -> runs 2, enters waitUntil
    CHECK(trace == std::vector<int>{1, 2});

    runner.update(0.016f); // still waiting on gateOpen
    CHECK(trace == std::vector<int>{1, 2});

    gateOpen = true;
    runner.update(0.016f); // gateOpen true -> runs 3, enters waitWhile(busy)
    CHECK(trace == std::vector<int>{1, 2, 3});

    busy = false;
    runner.update(0.016f); // busy false -> runs repeat(2)
    CHECK(trace == std::vector<int>{1, 2, 3, 4, 4});
    CHECK(runner.runningCount() == 0);
}

TEST_CASE("Stateful yieldStep coroutine supports Wait::nextFrame, seconds, and done") {
    CoroutineRunner runner;
    int stage = 0;
    std::vector<int> stages;

    runner.start([&](float /*dt*/) -> Wait {
        stages.push_back(stage);
        if (stage == 0) {
            stage = 1;
            return Wait::nextFrame();
        }
        if (stage == 1) {
            stage = 2;
            return Wait::seconds(0.5f);
        }
        return Wait::done();
    });

    runner.update(0.1f);
    CHECK(stages == std::vector<int>{0});

    runner.update(0.1f);
    CHECK(stages == std::vector<int>{0, 1});

    runner.update(0.25f);
    CHECK(stages == std::vector<int>{0, 1});

    runner.update(0.25f);
    CHECK(stages == std::vector<int>{0, 1, 2});
    CHECK(runner.runningCount() == 0);
}

TEST_CASE("Coroutine pause, resume, cancel, and re-entrancy") {
    CoroutineRunner runner;
    bool completed = false;
    bool cancelled = false;
    int childRan = 0;

    Coroutine script;
    script.then([&] {
              Coroutine child;
              child.then([&] { childRan++; });
              runner.start(std::move(child));
          })
        .waitSeconds(1.0f)
        .onComplete([&] { completed = true; })
        .onCancel([&] { cancelled = true; });

    CoroutineHandle handle = runner.start(std::move(script));
    runner.pause(handle);
    CHECK(runner.isPaused(handle));
    runner.update(0.5f);
    CHECK(childRan == 0);

    runner.resume(handle);
    runner.update(0.5f);
    CHECK(runner.isRunning(handle));

    // Child was spawned during update and runs on the next update.
    runner.update(0.1f);
    CHECK(childRan == 1);

    CHECK(runner.cancel(handle));
    CHECK(cancelled);
    CHECK_FALSE(completed);
    CHECK_FALSE(runner.isRunning(handle));
    CHECK_FALSE(runner.cancel(handle)); // Double cancel is a no-op
}
