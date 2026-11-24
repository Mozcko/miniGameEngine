#include <doctest/doctest.h>
#include <mge/core/Application.h>
#include <mge/core/Clock.h>

#include <memory>
#include <vector>

using namespace mge;

namespace {
    // Headless app with manual clock: each frame lasts exactly frameTime seconds
    class FixedStepApp : public Application {
    public:
        FixedStepApp(std::uint64_t frames, double frameTime, double tickRate = 60.0)
            : Application(makeConfig(frames, tickRate)) {
            setClock(std::make_unique<ManualClock>(frameTime));
        }

        int fixedSteps = 0;
        float lastFixedDt = 0.0f;
        std::vector<int> stepsPerFrame;
        std::vector<float> alphas;
        std::vector<float> frameDeltas;
        double simulatedPosition = 0.0; // State simulated only in fixed step
        std::uint64_t pauseAtFrame = 0; // 0 = do not pause

    protected:
        void onFixedUpdate(float fixedDt) override {
            fixedSteps++;
            m_stepsThisFrame++;
            lastFixedDt = fixedDt;
            simulatedPosition += 2.0 * fixedDt; // constant speed of 2 u/s
        }
        void onUpdate(float dt) override {
            stepsPerFrame.push_back(m_stepsThisFrame);
            m_stepsThisFrame = 0;
            frameDeltas.push_back(dt);
            alphas.push_back(time().alpha());
            if (pauseAtFrame > 0 && time().frameCount() + 1 == pauseAtFrame) time().setTimeScale(0.0f);
        }

    private:
        static ApplicationConfig makeConfig(std::uint64_t frames, double tickRate) {
            ApplicationConfig config;
            config.headless = true;
            config.maxFrames = frames;
            config.tickRate = tickRate;
            return config;
        }
        int m_stepsThisFrame = 0;
    };
}

TEST_CASE("At 60 Hz with 1/60 s frames there is exactly one fixed step per frame") {
    // The first frame measures time since loop start (1/60): also performs one step
    FixedStepApp app(100, 1.0 / 60.0);
    CHECK(app.run() == 0);
    CHECK(app.fixedSteps == 100);
    CHECK(app.lastFixedDt == doctest::Approx(1.0f / 60.0f));
    CHECK(app.time().fixedStepCount() == 100);
}

TEST_CASE("Fast frames (240 FPS) accumulate: one fixed step every 4 frames and alpha interpolates") {
    FixedStepApp app(8, 1.0 / 240.0);
    app.run();
    CHECK(app.fixedSteps == 2);
    std::vector<int> expected = {0, 0, 0, 1, 0, 0, 0, 1};
    CHECK(app.stepsPerFrame == expected);
    CHECK(app.alphas[0] == doctest::Approx(0.25f));
    CHECK(app.alphas[1] == doctest::Approx(0.5f));
    CHECK(app.alphas[2] == doctest::Approx(0.75f));
    CHECK(app.alphas[3] == doctest::Approx(0.0f).epsilon(1e-4));
}

TEST_CASE("Fixed-step simulation is FPS-independent") {
    // 1 simulated second at 30, 60, and 144 FPS: same step count and same final state
    FixedStepApp slow(30, 1.0 / 30.0);
    FixedStepApp normal(60, 1.0 / 60.0);
    FixedStepApp fast(144, 1.0 / 144.0);
    slow.run();
    normal.run();
    fast.run();
    CHECK(slow.fixedSteps == 60);
    CHECK(normal.fixedSteps == 60);
    CHECK(fast.fixedSteps == doctest::Approx(60).epsilon(0.02)); // ±1 step due to rounding
    CHECK(slow.simulatedPosition == doctest::Approx(2.0));
    CHECK(normal.simulatedPosition == doctest::Approx(2.0));
}

TEST_CASE("Same inputs, same result (deterministic)") {
    FixedStepApp a(50, 0.013);
    FixedStepApp b(50, 0.013);
    a.run();
    b.run();
    CHECK(a.stepsPerFrame == b.stepsPerFrame);
    CHECK(a.simulatedPosition == b.simulatedPosition);
}

TEST_CASE("Very slow frames: cap steps per frame and clamp frame delta to maxFrameTime") {
    // 1 s frames: clamped to 0.25 s = 15 steps, but cap is 5 per frame
    FixedStepApp app(3, 1.0);
    app.run();
    std::vector<int> expected = {5, 5, 5};
    CHECK(app.stepsPerFrame == expected);
    CHECK(app.frameDeltas[0] == doctest::Approx(0.25f));
    CHECK(app.time().unscaledElapsed() == doctest::Approx(0.75));
}

TEST_CASE("When paused (scale 0) there are no fixed steps and scaled delta is 0, while real time advances") {
    FixedStepApp app(10, 1.0 / 60.0);
    app.pauseAtFrame = 5; // Pauses at the end of frame 5
    app.run();
    CHECK(app.fixedSteps == 5);
    CHECK(app.frameDeltas[9] == doctest::Approx(0.0f));
    CHECK(app.time().unscaledDelta() == doctest::Approx(1.0f / 60.0f));
    CHECK(app.time().unscaledElapsed() == doctest::Approx(10.0 / 60.0));
    CHECK(app.time().elapsed() == doctest::Approx(5.0 / 60.0));
}

TEST_CASE("setTimeScale rejects negative values") {
    Time time;
    time.setTimeScale(-2.0f);
    CHECK(time.timeScale() == doctest::Approx(0.0f));
    time.setTimeScale(0.5f);
    CHECK(time.timeScale() == doctest::Approx(0.5f));
}

TEST_CASE("SteadyClock is monotonic") {
    SteadyClock clock;
    double a = clock.now();
    double b = clock.now();
    CHECK(a >= 0.0);
    CHECK(b >= a);
}
