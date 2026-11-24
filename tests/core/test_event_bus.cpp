#include <doctest/doctest.h>
#include <mge/core/EventBus.h>

#include <string>
#include <utility>
#include <vector>

using namespace mge;

namespace {
    struct DamageEvent {
        int amount = 0;
        std::string source;
    };

    struct ScoreEvent {
        int points = 0;
    };
}

TEST_CASE("EventBus: publish inmediato entrega eventos por tipo y orden de suscripción") {
    EventBus bus;
    std::vector<int> order;
    int totalScore = 0;

    bus.subscribe<DamageEvent>([&](const DamageEvent& ev) { order.push_back(ev.amount); });
    bus.subscribe<DamageEvent>([&](const DamageEvent& ev) { order.push_back(ev.amount * 10); });
    bus.subscribe<ScoreEvent>([&](const ScoreEvent& ev) { totalScore += ev.points; });

    CHECK(bus.subscriberCount<DamageEvent>() == 2);
    CHECK(bus.subscriberCount<ScoreEvent>() == 1);

    bus.publish(DamageEvent{5, "lava"});
    CHECK(order == std::vector<int>{5, 50});
    CHECK(totalScore == 0);

    bus.publish(ScoreEvent{100});
    CHECK(totalScore == 100);
}

TEST_CASE("EventBus: unsubscribe y ScopedSubscription cancelan listeners") {
    EventBus bus;
    int callsA = 0;
    int callsB = 0;

    const SubscriptionId idA = bus.subscribe<ScoreEvent>([&](const ScoreEvent&) { callsA++; });
    CHECK(idA.isValid());

    {
        ScopedSubscription scoped = bus.subscribeScoped<ScoreEvent>([&](const ScoreEvent&) { callsB++; });
        CHECK(scoped.isValid());
        CHECK(bus.subscriberCount<ScoreEvent>() == 2);

        bus.publish(ScoreEvent{1});
        CHECK(callsA == 1);
        CHECK(callsB == 1);
    }

    CHECK(bus.subscriberCount<ScoreEvent>() == 1);
    bus.publish(ScoreEvent{1});
    CHECK(callsA == 2);
    CHECK(callsB == 1);

    bus.unsubscribe(idA);
    CHECK(bus.subscriberCount<ScoreEvent>() == 0);
    bus.publish(ScoreEvent{1});
    CHECK(callsA == 2);

    // Cancelar un id inválido o ya cancelado no hace nada
    bus.unsubscribe(kInvalidSubscription);
    bus.unsubscribe(idA);
}

TEST_CASE("EventBus: ScopedSubscription movible y release()") {
    EventBus bus;
    int calls = 0;

    SubscriptionId releasedId = kInvalidSubscription;
    {
        ScopedSubscription first = bus.subscribeScoped<ScoreEvent>([&](const ScoreEvent&) { calls++; });
        ScopedSubscription moved = std::move(first);
        CHECK_FALSE(first.isValid());
        CHECK(moved.isValid());
        releasedId = moved.release();
        CHECK_FALSE(moved.isValid());
    }

    // Al haber usado release(), la suscripción sigue activa tras salir del ámbito
    CHECK(bus.subscriberCount<ScoreEvent>() == 1);
    bus.publish(ScoreEvent{10});
    CHECK(calls == 1);

    bus.unsubscribe(releasedId);
    CHECK(bus.subscriberCount<ScoreEvent>() == 0);
}

TEST_CASE("EventBus: enqueue y flush despachan en orden FIFO y difieren eventos anidados") {
    EventBus bus;
    std::vector<std::string> log;

    bus.subscribe<DamageEvent>([&](const DamageEvent& ev) {
        log.push_back("dmg:" + ev.source);
        if (ev.source == "first") bus.enqueue(ScoreEvent{42});
    });
    bus.subscribe<ScoreEvent>([&](const ScoreEvent& ev) {
        log.push_back("score:" + std::to_string(ev.points));
    });

    bus.enqueue(DamageEvent{10, "first"});
    bus.enqueue(DamageEvent{20, "second"});
    CHECK(bus.queuedCount() == 2);
    CHECK(log.empty());

    bus.flush();
    CHECK(log == std::vector<std::string>{"dmg:first", "dmg:second"});
    // El evento encolado durante flush() espera al siguiente flush()
    CHECK(bus.queuedCount() == 1);

    bus.flush();
    CHECK(log == std::vector<std::string>{"dmg:first", "dmg:second", "score:42"});
    CHECK(bus.queuedCount() == 0);
}

TEST_CASE("EventBus: reentrada segura (desuscribir, suscribir y publicar durante publish)") {
    EventBus bus;
    int callsSelf = 0;
    int callsOther = 0;
    int callsLate = 0;

    SubscriptionId selfId = kInvalidSubscription;
    SubscriptionId otherId = kInvalidSubscription;

    selfId = bus.subscribe<ScoreEvent>([&](const ScoreEvent& ev) {
        callsSelf++;
        bus.unsubscribe(selfId);
        bus.unsubscribe(otherId);
        bus.subscribe<ScoreEvent>([&](const ScoreEvent&) { callsLate++; });
        if (ev.points == 1) bus.publish(ScoreEvent{2});
    });
    otherId = bus.subscribe<ScoreEvent>([&](const ScoreEvent&) { callsOther++; });

    bus.publish(ScoreEvent{1});
    // selfId se ejecutó una vez; otherId fue cancelado antes de correr; callsLate solo recibió el evento 2
    CHECK(callsSelf == 1);
    CHECK(callsOther == 0);
    CHECK(callsLate == 1);
    CHECK(bus.subscriberCount<ScoreEvent>() == 1);

    bus.clear();
    CHECK(bus.subscriberCount<ScoreEvent>() == 0);
    CHECK(bus.queuedCount() == 0);
}
