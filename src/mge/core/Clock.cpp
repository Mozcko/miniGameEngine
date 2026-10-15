#include <mge/core/Clock.h>

#include <chrono>

namespace mge {

    namespace {
        double steadySeconds() {
            using namespace std::chrono;
            return duration<double>(steady_clock::now().time_since_epoch()).count();
        }
    }

    SteadyClock::SteadyClock() : m_origin(steadySeconds()) {}

    double SteadyClock::now() {
        return steadySeconds() - m_origin;
    }
}
