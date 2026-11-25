#include <doctest/doctest.h>
#include <mge/core/EventBus.h>

#include <string>
#include <utility>
#include <vector>

using namespace mge;

namespace {
    struct DamageEvent {
        int amount = 0;
    };

    struct ScoreEvent {
        int points = 0;
        std::string reason;
    };
}

TEST_CASE("EventBus subscribe, emit, and unsubscribe isolate event types") {
    EventBus bus;
    int totalDamage = 0;
    int totalScore = 0;

    SubscriptionHandle h1 =
        bus.subscribe<DamageEvent>([&](const DamageEvent& e) { totalDamage += e.amount; });
    SubscriptionHandle h2 =
        bus.subscribe<DamageEvent>([&](const DamageEvent& e) { totalDamage += e.amount * 2; });
    SubscriptionHandle h3 =
        bus.subscribe<ScoreEvent>([&](const ScoreEvent& e) { totalScore += e.points; });

    CHECK(h1.isValid());
    CHECK(h2.isValid());
    CHECK(h3.isValid());
    CHECK(bus.subscriberCount<DamageEvent>() == 2);
    CHECK(bus.subscriberCount<ScoreEvent>() == 1);

    bus.emit(DamageEvent{10});
    CHECK(totalDamage == 30);
    CHECK(totalScore == 0);

    bus.emit(ScoreEvent{50, "coin"});
    CHECK(totalScore == 50);

    CHECK(bus.unsubscribe(h2));
    CHECK_FALSE(bus.unsubscribe(h2));
    CHECK_FALSE(bus.unsubscribe(kInvalidSubscription));
    CHECK(bus.subscriberCount<DamageEvent>() == 1);

    bus.emit(DamageEvent{5});
    CHECK(totalDamage == 35);
}

TEST_CASE("EventBus subscribeOnce fires once and ScopedSubscription unsubscribes on scope exit") {
    EventBus bus;
    int onceCount = 0;
    int scopedCount = 0;

    bus.subscribeOnce<DamageEvent>([&](const DamageEvent&) { onceCount++; });
    CHECK(bus.subscriberCount<DamageEvent>() == 1);
    bus.emit(DamageEvent{1});
    bus.emit(DamageEvent{1});
    CHECK(onceCount == 1);
    CHECK(bus.subscriberCount<DamageEvent>() == 0);

    {
        ScopedSubscription sub =
            bus.subscribeScoped<DamageEvent>([&](const DamageEvent&) { scopedCount++; });
        ScopedSubscription moved = std::move(sub);
        CHECK_FALSE(sub.isValid());
        CHECK(moved.isValid());
        CHECK(bus.subscriberCount<DamageEvent>() == 1);

        bus.emit(DamageEvent{1});
        CHECK(scopedCount == 1);
    }

    CHECK(bus.subscriberCount<DamageEvent>() == 0);
    bus.emit(DamageEvent{1});
    CHECK(scopedCount == 1);
}

TEST_CASE("EventBus enqueue and flush preserve FIFO order and defer newly queued events") {
    EventBus bus;
    std::vector<std::string> order;

    bus.subscribe<DamageEvent>([&](const DamageEvent& e) {
        order.push_back("dmg:" + std::to_string(e.amount));
        if (e.amount == 1) {
            // Enqueued during flush: must wait until the next flush().
            bus.enqueue(DamageEvent{99});
        }
    });
    bus.subscribe<ScoreEvent>([&](const ScoreEvent& e) { order.push_back("score:" + e.reason); });

    bus.enqueue(DamageEvent{1});
    bus.enqueue(ScoreEvent{10, "a"});
    bus.enqueue(DamageEvent{2});
    CHECK(bus.queuedCount() == 3);
    CHECK(order.empty());

    bus.flush();
    CHECK(order == std::vector<std::string>{"dmg:1", "score:a", "dmg:2"});
    CHECK(bus.queuedCount() == 1);

    bus.flush();
    CHECK(order == std::vector<std::string>{"dmg:1", "score:a", "dmg:2", "dmg:99"});
    CHECK(bus.queuedCount() == 0);
}

TEST_CASE("EventBus is re-entrancy safe during emit") {
    EventBus bus;
    std::vector<int> calls;
    SubscriptionHandle self = kInvalidSubscription;
    SubscriptionHandle second = kInvalidSubscription;

    self = bus.subscribe<DamageEvent>([&](const DamageEvent& e) {
        calls.push_back(1);
        bus.unsubscribe(self);
        bus.unsubscribe(second);
        bus.subscribe<DamageEvent>([&](const DamageEvent&) { calls.push_back(3); });
        if (e.amount > 0) {
            bus.emit(DamageEvent{0}); // Recursive emit
        }
    });
    second = bus.subscribe<DamageEvent>([&](const DamageEvent&) { calls.push_back(2); });

    bus.emit(DamageEvent{1});
    // self ran once, cancelled itself and second; new subscriber (3) only becomes active after emit.
    CHECK(calls == std::vector<int>{1});
    CHECK(bus.subscriberCount<DamageEvent>() == 1);

    bus.emit(DamageEvent{0});
    CHECK(calls == std::vector<int>{1, 3});

    bus.clear();
    CHECK(bus.subscriberCount<DamageEvent>() == 0);
}
