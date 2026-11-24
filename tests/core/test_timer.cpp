#include <doctest/doctest.h>
#include <mge/core/Timer.h>

using namespace mge;

TEST_CASE("TimerManager: after dispara una sola vez tras la demora") {
    TimerManager timers;
    int count = 0;
    bool activeDuringCallback = true;

    TimerHandle handle = kNullTimer;
    handle = timers.after(0.5f, [&]() {
        count++;
        activeDuringCallback = timers.isActive(handle);
    });

    CHECK(timers.isActive(handle));
    CHECK(timers.activeCount() == 1);
    CHECK(timers.remaining(handle) == doctest::Approx(0.5f));

    timers.update(0.25f);
    CHECK(count == 0);
    CHECK(timers.remaining(handle) == doctest::Approx(0.25f));

    timers.update(0.25f);
    CHECK(count == 1);
    CHECK_FALSE(activeDuringCallback);
    CHECK_FALSE(timers.isActive(handle));
    CHECK(timers.activeCount() == 0);

    timers.update(1.0f);
    CHECK(count == 1);
}

TEST_CASE("TimerManager: every repite indefinidamente o un número fijo de veces sin deriva") {
    TimerManager timers;
    int infiniteTicks = 0;
    int limitedTicks = 0;

    const TimerHandle inf = timers.every(0.2f, [&]() { infiniteTicks++; });
    const TimerHandle lim = timers.every(0.25f, [&]() { limitedTicks++; }, 3);

    CHECK(timers.activeCount() == 2);

    // Un dt grande (0.65 s) dispara 3 veces el de 0.2 s y 2 veces el de 0.25 s
    timers.update(0.65f);
    CHECK(infiniteTicks == 3);
    CHECK(limitedTicks == 2);
    CHECK(timers.isActive(inf));
    CHECK(timers.isActive(lim));

    // Quedan 0.15 s en inf y 0.10 s en lim
    timers.update(0.20f);
    CHECK(infiniteTicks == 4);
    CHECK(limitedTicks == 3);
    CHECK(timers.isActive(inf));
    CHECK_FALSE(timers.isActive(lim));
    CHECK(timers.activeCount() == 1);

    // Intervalos <= 0 o repeatCount == 0 devuelven kNullTimer
    CHECK(timers.every(0.0f, [&]() {}).isNull());
    CHECK(timers.every(-1.0f, [&]() {}).isNull());
    CHECK(timers.every(0.5f, [&]() {}, 0).isNull());
}

TEST_CASE("TimerManager: pause, resume, cancel y reciclaje generacional de handles") {
    TimerManager timers;
    int count = 0;

    const TimerHandle first = timers.after(1.0f, [&]() { count++; });
    timers.update(0.4f);
    timers.pause(first);
    CHECK(timers.isPaused(first));

    timers.update(5.0f);
    CHECK(count == 0);
    CHECK(timers.remaining(first) == doctest::Approx(0.6f));

    timers.resume(first);
    CHECK_FALSE(timers.isPaused(first));
    timers.update(0.3f);
    CHECK(timers.remaining(first) == doctest::Approx(0.3f));

    timers.cancel(first);
    CHECK_FALSE(timers.isActive(first));
    CHECK(timers.activeCount() == 0);

    // Al crear un nuevo temporizador se recicla el índice con nueva generación
    const TimerHandle second = timers.after(1.0f, [&]() { count += 10; });
    CHECK(second.index == first.index);
    CHECK(second.generation != first.generation);
    CHECK_FALSE(timers.isActive(first));
    CHECK(timers.isActive(second));

    // Cancelar el handle viejo no afecta al nuevo
    timers.cancel(first);
    CHECK(timers.isActive(second));

    timers.update(1.0f);
    CHECK(count == 10);
}

TEST_CASE("TimerManager: callbacks reentrantes (crear, cancelar otro, clear)") {
    TimerManager timers;
    int firstCalls = 0;
    int secondCalls = 0;
    int spawnedCalls = 0;

    TimerHandle second = kNullTimer;
    timers.after(0.1f, [&]() {
        firstCalls++;
        timers.cancel(second);
        timers.after(0.0f, [&]() { spawnedCalls++; });
    });
    second = timers.after(0.1f, [&]() { secondCalls++; });

    timers.update(0.1f);
    CHECK(firstCalls == 1);
    CHECK(secondCalls == 0);
    // El temporizador creado dentro del callback no dispara en el mismo update
    CHECK(spawnedCalls == 0);
    CHECK(timers.activeCount() == 1);

    timers.update(0.0f);
    CHECK(spawnedCalls == 1);
    CHECK(timers.activeCount() == 0);
}
