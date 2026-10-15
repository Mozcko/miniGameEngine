#include <doctest/doctest.h>
#include <mge/core/Time.h>

using namespace MiniGameEngine;

// Time es estático: cada test deja la escala en 1 para no afectar a los demás
struct TimeScaleGuard {
    ~TimeScaleGuard() { Time::setTimeScale(1.0f); }
};

TEST_CASE("advance aplica la escala de tiempo") {
    TimeScaleGuard guard;
    Time::setTimeScale(0.5f);
    Time::advance(0.02f);
    CHECK(Time::getUnscaledDeltaTime() == doctest::Approx(0.02f));
    CHECK(Time::getDeltaTime() == doctest::Approx(0.01f));
}

TEST_CASE("En pausa (escala 0) el delta escalado es 0 pero el real no") {
    TimeScaleGuard guard;
    Time::setTimeScale(0.0f);
    Time::advance(0.016f);
    CHECK(Time::getDeltaTime() == doctest::Approx(0.0f));
    CHECK(Time::getUnscaledDeltaTime() == doctest::Approx(0.016f));
}

TEST_CASE("Los saltos grandes se recortan a kMaxDeltaTime") {
    TimeScaleGuard guard;
    Time::advance(5.0f);
    CHECK(Time::getUnscaledDeltaTime() == doctest::Approx(Time::kMaxDeltaTime));
    CHECK(Time::getDeltaTime() == doctest::Approx(Time::kMaxDeltaTime));
}

TEST_CASE("Un delta negativo se trata como 0") {
    TimeScaleGuard guard;
    Time::advance(-1.0f);
    CHECK(Time::getUnscaledDeltaTime() == doctest::Approx(0.0f));
}

TEST_CASE("update() usa el reloj real y nunca da valores negativos") {
    TimeScaleGuard guard;
    Time::init();
    Time::update();
    CHECK(Time::getUnscaledDeltaTime() >= 0.0f);
    CHECK(Time::getUnscaledDeltaTime() <= Time::kMaxDeltaTime);
}
