#include <doctest/doctest.h>
#include <mge/core/Timer.h>

#include <vector>

using namespace mge;

TEST_CASE("One-shot Timer fires once, reports progress, and resets cleanly") {
    Timer timer(1.0f, false);
    CHECK_FALSE(timer.isFinished());
    CHECK(timer.remaining() == doctest::Approx(1.0f));
    CHECK(timer.progress() == doctest::Approx(0.0f));

    CHECK_FALSE(timer.update(0.4f));
    CHECK(timer.elapsed() == doctest::Approx(0.4f));
    CHECK(timer.remaining() == doctest::Approx(0.6f));
    CHECK(timer.progress() == doctest::Approx(0.4f));

    // Paused or non-positive dt does not advance.
    timer.pause();
    CHECK(timer.isPaused());
    CHECK_FALSE(timer.update(1.0f));
    CHECK(timer.elapsed() == doctest::Approx(0.4f));
    timer.resume();
    CHECK_FALSE(timer.update(-0.5f));
    CHECK_FALSE(timer.update(0.0f));

    CHECK(timer.update(0.6f));
    CHECK(timer.isFinished());
    CHECK(timer.remaining() == doctest::Approx(0.0f));
    CHECK(timer.progress() == doctest::Approx(1.0f));

    // Already finished: does not fire again until reset.
    CHECK_FALSE(timer.update(1.0f));

    timer.reset(0.5f);
    CHECK_FALSE(timer.isFinished());
    CHECK(timer.duration() == doctest::Approx(0.5f));
    CHECK(timer.update(0.5f));
}

TEST_CASE("Repeating Timer counts multiple fires and preserves remainder") {
    Timer timer(0.25f, true);
    CHECK(timer.isRepeating());
    CHECK(timer.updateCount(0.6f) == 2);
    CHECK_FALSE(timer.isFinished());
    CHECK(timer.elapsed() == doctest::Approx(0.1f));

    CHECK(timer.updateCount(0.15f) == 1);
    CHECK(timer.elapsed() == doctest::Approx(0.0f).epsilon(1e-5));
}

TEST_CASE("Cooldown readiness, trigger, update, and progress") {
    Cooldown cd(2.0f);
    CHECK(cd.isReady());
    CHECK(cd.progress() == doctest::Approx(1.0f));

    CHECK(cd.tryTrigger());
    CHECK_FALSE(cd.isReady());
    CHECK_FALSE(cd.tryTrigger());
    CHECK(cd.remaining() == doctest::Approx(2.0f));
    CHECK(cd.progress() == doctest::Approx(0.0f));

    cd.update(0.5f);
    CHECK(cd.remaining() == doctest::Approx(1.5f));
    CHECK(cd.progress() == doctest::Approx(0.25f));

    cd.update(2.0f);
    CHECK(cd.isReady());
    CHECK(cd.remaining() == doctest::Approx(0.0f));

    Cooldown notReady(1.0f, false);
    CHECK_FALSE(notReady.isReady());
    notReady.reset();
    CHECK(notReady.isReady());
}

TEST_CASE("TimerManager schedules one-shot, repeating, pause, and cancel") {
    TimerManager manager;
    int oneShotCount = 0;
    int repeatCount = 0;

    TimerHandle once = manager.after(0.5f, [&] { oneShotCount++; });
    TimerHandle rep = manager.every(0.25f, [&] { repeatCount++; }, 3);
    CHECK(once.isValid());
    CHECK(rep.isValid());
    CHECK(manager.activeCount() == 2);
    CHECK(manager.isActive(once));
    CHECK(manager.remaining(once) == doctest::Approx(0.5f));

    manager.pause(once);
    CHECK(manager.isPaused(once));
    manager.update(0.5f);
    CHECK(oneShotCount == 0);
    CHECK(repeatCount == 2);

    manager.resume(once);
    manager.update(0.5f);
    CHECK(oneShotCount == 1);
    CHECK(repeatCount == 3);
    CHECK_FALSE(manager.isActive(once));
    CHECK_FALSE(manager.isActive(rep));
    CHECK(manager.activeCount() == 0);

    int cancelledFired = 0;
    TimerHandle toCancel = manager.after(0.1f, [&] { cancelledFired++; });
    CHECK(manager.cancel(toCancel));
    CHECK_FALSE(manager.cancel(toCancel));
    manager.update(1.0f);
    CHECK(cancelledFired == 0);
}

TEST_CASE("TimerManager is re-entrancy safe during callbacks") {
    TimerManager manager;
    std::vector<int> log;
    TimerHandle other = kInvalidTimer;
    TimerHandle self = kInvalidTimer;

    self = manager.every(0.1f, [&] {
        log.push_back(1);
        manager.cancel(self);
        manager.cancel(other);
        manager.after(0.1f, [&] { log.push_back(3); });
    });
    other = manager.after(0.1f, [&] { log.push_back(2); });

    manager.update(0.3f); // self fires once, cancels self & other, schedules new timer
    CHECK(log == std::vector<int>{1});
    CHECK(manager.activeCount() == 1);

    manager.update(0.1f);
    CHECK(log == std::vector<int>{1, 3});
    CHECK(manager.activeCount() == 0);
}
