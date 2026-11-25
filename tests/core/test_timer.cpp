#include <doctest/doctest.h>
#include <mge/core/Application.h>
#include <mge/core/Clock.h>
#include <mge/core/Timer.h>

#include <limits>
#include <memory>

using namespace mge;

TEST_CASE("Default-constructed Timer is idle until started") {
    Timer timer;
    CHECK_FALSE(timer.isRunning());
    CHECK_FALSE(timer.isFinished());
    CHECK_FALSE(timer.isRepeating());
    CHECK(timer.duration() == doctest::Approx(0.0f));
    CHECK(timer.elapsed() == doctest::Approx(0.0f));
    CHECK(timer.remaining() == doctest::Approx(0.0f));
    CHECK(timer.progress() == doctest::Approx(0.0f));
    CHECK_FALSE(timer.tick(1.0f));
    CHECK_FALSE(timer.isFinished());

    timer.start(2.0f);
    CHECK(timer.isRunning());
    CHECK(timer.duration() == doctest::Approx(2.0f));
    CHECK_FALSE(timer.tick(1.0f));
    CHECK(timer.tick(1.0f));
    CHECK(timer.isFinished());
}

TEST_CASE("One-shot Timer fires once at duration and stays finished") {
    Timer timer = Timer::oneShot(1.0f);
    CHECK(timer.isRunning());
    CHECK_FALSE(timer.isRepeating());
    CHECK_FALSE(timer.isFinished());

    CHECK_FALSE(timer.tick(0.25f));
    CHECK(timer.elapsed() == doctest::Approx(0.25f));
    CHECK(timer.remaining() == doctest::Approx(0.75f));
    CHECK(timer.progress() == doctest::Approx(0.25f));

    CHECK_FALSE(timer.tick(0.5f));
    CHECK(timer.progress() == doctest::Approx(0.75f));

    // Overshoot finishes and clamps elapsed to duration
    CHECK(timer.tick(0.5f));
    CHECK(timer.isFinished());
    CHECK_FALSE(timer.isRunning());
    CHECK(timer.elapsed() == doctest::Approx(1.0f));
    CHECK(timer.remaining() == doctest::Approx(0.0f));
    CHECK(timer.progress() == doctest::Approx(1.0f));

    // Subsequent ticks do not re-trigger
    CHECK_FALSE(timer.tick(1.0f));
    CHECK(timer.tickCount(1.0f) == 0);
}

TEST_CASE("Repeating Timer triggers periodically without losing overflow") {
    Timer timer = Timer::repeating(0.5f);
    CHECK(timer.isRunning());
    CHECK(timer.isRepeating());

    CHECK(timer.tickCount(0.3f) == 0);
    CHECK(timer.tickCount(0.3f) == 1);
    CHECK(timer.isRunning());
    CHECK_FALSE(timer.isFinished());
    CHECK(timer.elapsed() == doctest::Approx(0.1f));

    // Large dt triggers multiple times in one tick and preserves remainder
    CHECK(timer.tickCount(1.25f) == 2);
    CHECK(timer.elapsed() == doctest::Approx(0.35f));

    // 0.35 + 0.15 = 0.50 -> triggers again
    CHECK(timer.tick(0.15f));
    CHECK(timer.elapsed() == doctest::Approx(0.0f).epsilon(1e-5));
}

TEST_CASE("Timer pause, resume, stop, reset, and finish transitions") {
    Timer timer(2.0f);
    CHECK_FALSE(timer.tick(0.5f));
    timer.pause();
    CHECK_FALSE(timer.isRunning());
    CHECK_FALSE(timer.tick(5.0f));
    CHECK(timer.elapsed() == doctest::Approx(0.5f));

    timer.resume();
    CHECK(timer.isRunning());
    CHECK_FALSE(timer.tick(0.5f));
    CHECK(timer.elapsed() == doctest::Approx(1.0f));

    timer.stop();
    CHECK_FALSE(timer.isRunning());
    CHECK_FALSE(timer.isFinished());
    CHECK(timer.elapsed() == doctest::Approx(0.0f));

    timer.start();
    CHECK(timer.isRunning());
    timer.finish();
    CHECK(timer.isFinished());
    CHECK_FALSE(timer.isRunning());
    CHECK(timer.progress() == doctest::Approx(1.0f));

    // resume() on a finished timer keeps it finished; start() restarts it
    timer.resume();
    CHECK_FALSE(timer.isRunning());
    timer.start();
    CHECK(timer.isRunning());
    CHECK_FALSE(timer.isFinished());
    CHECK(timer.elapsed() == doctest::Approx(0.0f));

    timer.reset(0.5f);
    CHECK(timer.duration() == doctest::Approx(0.5f));
    CHECK(timer.tick(0.5f));
    timer.reset();
    CHECK(timer.isRunning());
    CHECK_FALSE(timer.isFinished());
    CHECK(timer.elapsed() == doctest::Approx(0.0f));
}

TEST_CASE("Timer handles zero/negative dt and zero/negative duration safely") {
    Timer timer(1.0f);
    CHECK_FALSE(timer.tick(0.0f));
    CHECK_FALSE(timer.tick(-0.5f));
    CHECK_FALSE(timer.tick(std::numeric_limits<float>::quiet_NaN()));
    CHECK(timer.elapsed() == doctest::Approx(0.0f));

    Timer zeroOneShot(-1.0f);
    CHECK(zeroOneShot.duration() == doctest::Approx(0.0f));
    CHECK(zeroOneShot.progress() == doctest::Approx(0.0f));
    CHECK_FALSE(zeroOneShot.tick(0.0f));
    CHECK(zeroOneShot.tick(0.016f));
    CHECK(zeroOneShot.isFinished());
    CHECK(zeroOneShot.progress() == doctest::Approx(1.0f));
    CHECK_FALSE(zeroOneShot.tick(0.016f));

    Timer zeroRepeating = Timer::repeating(0.0f);
    CHECK(zeroRepeating.tickCount(0.016f) == 1);
    CHECK(zeroRepeating.isRunning());
    CHECK_FALSE(zeroRepeating.isFinished());

    Timer shrink(2.0f);
    shrink.tick(1.5f);
    shrink.setDuration(1.0f);
    CHECK(shrink.elapsed() == doctest::Approx(1.0f));
    CHECK(shrink.remaining() == doctest::Approx(0.0f));
    shrink.setRepeating(true);
    CHECK(shrink.isRepeating());
}

TEST_CASE("Cooldown starts ready, consumes on tryUse, and recharges via tick") {
    Cooldown cd(1.0f);
    CHECK(cd.isReady());
    CHECK(cd.duration() == doctest::Approx(1.0f));
    CHECK(cd.remaining() == doctest::Approx(0.0f));
    CHECK(cd.elapsed() == doctest::Approx(1.0f));
    CHECK(cd.progress() == doctest::Approx(1.0f));
    CHECK_FALSE(cd.tick(0.5f)); // Already ready

    CHECK(cd.tryUse());
    CHECK_FALSE(cd.isReady());
    CHECK_FALSE(cd.tryUse());
    CHECK(cd.remaining() == doctest::Approx(1.0f));
    CHECK(cd.elapsed() == doctest::Approx(0.0f));
    CHECK(cd.progress() == doctest::Approx(0.0f));

    CHECK_FALSE(cd.tick(0.4f));
    CHECK(cd.remaining() == doctest::Approx(0.6f));
    CHECK(cd.elapsed() == doctest::Approx(0.4f));
    CHECK(cd.progress() == doctest::Approx(0.4f));

    // Returns true on the tick it becomes ready, false thereafter
    CHECK(cd.tick(0.6f));
    CHECK(cd.isReady());
    CHECK(cd.remaining() == doctest::Approx(0.0f));
    CHECK(cd.progress() == doctest::Approx(1.0f));
    CHECK_FALSE(cd.tick(0.1f));
}

TEST_CASE("Cooldown supports startReady=false, trigger, reset, setDuration, and edge cases") {
    Cooldown cd(2.0f, /*startReady=*/false);
    CHECK_FALSE(cd.isReady());
    CHECK(cd.remaining() == doctest::Approx(2.0f));

    CHECK_FALSE(cd.tick(0.0f));
    CHECK_FALSE(cd.tick(-1.0f));
    CHECK_FALSE(cd.tick(std::numeric_limits<float>::quiet_NaN()));
    CHECK(cd.remaining() == doctest::Approx(2.0f));

    cd.tick(1.0f);
    CHECK(cd.remaining() == doctest::Approx(1.0f));
    cd.trigger(); // Refreshes back to full duration
    CHECK(cd.remaining() == doctest::Approx(2.0f));

    cd.setDuration(0.5f); // Clamps active remaining to new duration
    CHECK(cd.remaining() == doctest::Approx(0.5f));
    CHECK(cd.progress() == doctest::Approx(0.0f));

    cd.reset();
    CHECK(cd.isReady());
    CHECK(cd.remaining() == doctest::Approx(0.0f));

    Cooldown zeroCd(-0.5f);
    CHECK(zeroCd.duration() == doctest::Approx(0.0f));
    CHECK(zeroCd.isReady());
    CHECK(zeroCd.progress() == doctest::Approx(1.0f));
    CHECK(zeroCd.tryUse());
    CHECK(zeroCd.isReady());
    CHECK(zeroCd.tryUse());
}

TEST_CASE("Timer and Cooldown work deterministically in a fixed-timestep Application") {
    class GameplayApp : public Application {
    public:
        GameplayApp() : Application(makeConfig()) { setClock(std::make_unique<ManualClock>(1.0 / 60.0)); }

        int spawns = 0;
        int shots = 0;

    protected:
        void onFixedUpdate(float dt) override {
            spawns += m_spawnTimer.tickCount(dt);
            m_fireCooldown.tick(dt);
            if (m_fireCooldown.tryUse()) shots++;
        }

    private:
        static ApplicationConfig makeConfig() {
            ApplicationConfig config;
            config.headless = true;
            config.maxFrames = 60; // 1.0 s at 60 Hz
            config.tickRate = 60.0;
            return config;
        }

        Timer m_spawnTimer = Timer::repeating(0.25f);
        Cooldown m_fireCooldown{0.5f};
    };

    GameplayApp app;
    CHECK(app.run() == 0);
    // 1.0 s with 0.25 s repeating timer: fires at 0.25, 0.50, 0.75, 1.00 (float accumulation -> 3 or 4)
    CHECK(app.spawns >= 3);
    CHECK(app.spawns <= 4);
    // Fires immediately on step 1 (t=1/60), then after 0.5 s (step 31)
    CHECK(app.shots == 2);
}
