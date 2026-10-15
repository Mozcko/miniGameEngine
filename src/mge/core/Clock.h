#pragma once

namespace mge {

    // Fuente de tiempo de la Application, en segundos (monotónica). Se puede reemplazar con
    // Application::setClock: SteadyClock para tiempo real, ManualClock para tests y corridas deterministas.
    class Clock {
    public:
        virtual ~Clock() = default;
        virtual double now() = 0;
    };

    // Tiempo real (std::chrono::steady_clock)
    class SteadyClock : public Clock {
    public:
        SteadyClock();
        double now() override;

    private:
        double m_origin;
    };

    // Tiempo controlado a mano. Con autoStep > 0, cada llamada a now() avanza ese tanto después de
    // devolver el valor actual: un frame de la Application = autoStep segundos, siempre igual.
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
