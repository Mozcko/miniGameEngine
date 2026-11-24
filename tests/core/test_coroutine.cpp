#include <doctest/doctest.h>
#include <mge/core/Coroutine.h>

#include <string>
#include <vector>

using namespace mge;

TEST_CASE("CoroutineRunner: CoroutineSequence con acciones, esperas por tiempo y frames") {
    CoroutineRunner runner;
    std::vector<int> trace;

    const CoroutineHandle handle = runner.start(CoroutineSequence()
                                                    .run([&]() { trace.push_back(1); })
                                                    .run([&]() { trace.push_back(2); })
                                                    .waitSeconds(0.5f)
                                                    .run([&]() { trace.push_back(3); })
                                                    .waitFrames(2)
                                                    .run([&]() { trace.push_back(4); }));

    CHECK(runner.isRunning(handle));
    CHECK(runner.runningCount() == 1);

    // Primer update: ejecuta las acciones consecutivas 1 y 2 y entra en waitSeconds(0.5)
    runner.update(0.1f);
    CHECK(trace == std::vector<int>{1, 2});

    runner.update(0.3f);
    CHECK(trace == std::vector<int>{1, 2});

    // Cumple los 0.5 s: ejecuta 3 y entra en waitFrames(2)
    runner.update(0.2f);
    CHECK(trace == std::vector<int>{1, 2, 3});

    runner.update(0.016f); // frame 1 de 2
    CHECK(trace == std::vector<int>{1, 2, 3});

    runner.update(0.016f); // frame 2 de 2 -> ejecuta 4 y termina
    CHECK(trace == std::vector<int>{1, 2, 3, 4});
    CHECK_FALSE(runner.isRunning(handle));
    CHECK(runner.runningCount() == 0);
}

TEST_CASE("CoroutineRunner: waitUntil, waitWhile y yieldFrame") {
    CoroutineRunner runner;
    bool gateOpen = false;
    bool busy = true;
    int stage = 0;

    const CoroutineHandle handle = runner.start(CoroutineSequence()
                                                    .run([&]() { stage = 1; })
                                                    .waitUntil([&]() { return gateOpen; })
                                                    .run([&]() { stage = 2; })
                                                    .waitWhile([&]() { return busy; })
                                                    .run([&]() { stage = 3; })
                                                    .yieldFrame()
                                                    .run([&]() { stage = 4; }));

    runner.update(0.016f);
    CHECK(stage == 1);

    runner.update(0.016f);
    CHECK(stage == 1);

    gateOpen = true;
    runner.update(0.016f);
    CHECK(stage == 2);

    runner.update(0.016f);
    CHECK(stage == 2);

    busy = false;
    runner.update(0.016f);
    CHECK(stage == 3);
    CHECK(runner.isRunning(handle));

    // Tras yieldFrame(), el siguiente update ejecuta stage = 4 y finaliza
    runner.update(0.016f);
    CHECK(stage == 4);
    CHECK_FALSE(runner.isRunning(handle));
}

TEST_CASE("CoroutineRunner: corrutina basada en función de paso manual (CoroutineStep)") {
    CoroutineRunner runner;
    int stepIndex = 0;
    std::vector<std::string> events;

    const CoroutineHandle handle = runner.start([&]() -> YieldInstruction {
        if (stepIndex == 0) {
            events.push_back("inicio");
            stepIndex++;
            return Wait::none();
        }
        if (stepIndex == 1) {
            events.push_back("fin");
            stepIndex++;
            return Wait::done();
        }
        return Wait::done();
    });

    runner.update(0.016f);
    CHECK(events == std::vector<std::string>{"inicio"});
    CHECK(runner.isRunning(handle));

    runner.update(0.016f);
    CHECK(events == std::vector<std::string>{"inicio", "fin"});
    CHECK_FALSE(runner.isRunning(handle));
}

TEST_CASE("CoroutineRunner: pause, resume, stop y reentrada segura") {
    CoroutineRunner runner;
    int counter = 0;
    int childDone = 0;

    const CoroutineHandle handle = runner.start(CoroutineSequence()
                                                    .waitSeconds(0.5f)
                                                    .run([&]() {
                                                        counter++;
                                                        runner.start(CoroutineSequence().run(
                                                            [&]() { childDone++; }));
                                                    })
                                                    .waitSeconds(0.5f)
                                                    .run([&]() { counter++; }));

    runner.update(0.016f); // inicia la espera de 0.5 s
    runner.pause(handle);
    CHECK(runner.isPaused(handle));

    runner.update(2.0f);
    CHECK(counter == 0);

    runner.resume(handle);
    CHECK_FALSE(runner.isPaused(handle));
    runner.update(0.5f);
    CHECK(counter == 1);
    // La corrutina hija arranca en el siguiente update
    CHECK(childDone == 0);

    runner.stop(handle);
    CHECK_FALSE(runner.isRunning(handle));

    runner.update(1.0f);
    CHECK(counter == 1);
    CHECK(childDone == 1);
    CHECK(runner.runningCount() == 0);
}
