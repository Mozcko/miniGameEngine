#pragma once

namespace mge {

    // Time source for Application, in seconds (monotonic). Can be replaced via
    // Application::setClock: SteadyClock for real time, ManualClock for tests and deterministic runs.
    class Clock {
    public:
        virtual ~Clock() = default;
        virtual double now() = 0;
    };

    // Real time (std::chrono::steady_clock)
    class SteadyClock : public Clock {
    public:
        SteadyClock();
        double now() override;

    private:
        double m_origin;
    };

    // Manually controlled time. With autoStep > 0, each call to now() advances by that amount after
    // returning the current value: one Application frame = autoStep seconds, always identical.
    class ManualClock : public Clock {
    public:
        explicit ManualClock(double autoStep = 0.0) : m_autoStep(autoStep) {}

        double now() override {
            double current = m_time;
            m_time += m_autoStep;
            return current;
        }
        void advance(double seconds) { m_time += seconds; }
        void setAutoStep(double seconds) { m_autoStep = seconds; }

    private:
        double m_time = 0.0;
        double m_autoStep;
    };
}
