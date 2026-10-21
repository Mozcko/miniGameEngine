#include <doctest/doctest.h>
#include <mge/core/SpscQueue.h>

#include <cstdint>
#include <thread>

using namespace mge;

TEST_CASE("SpscQueue: FIFO, llena con Capacity - 1 y vacía") {
    SpscQueue<int, 8> queue;
    CHECK(queue.empty());
    CHECK(SpscQueue<int, 8>::capacity() == 7);
    int value = 0;
    CHECK_FALSE(queue.pop(value));
    for (int i = 0; i < 7; i++) CHECK(queue.push(i));
    CHECK_FALSE(queue.push(99)); // Llena
    for (int i = 0; i < 7; i++) {
        REQUIRE(queue.pop(value));
        CHECK(value == i);
    }
    CHECK(queue.empty());
    // Da la vuelta al anillo varias veces
    for (int round = 0; round < 20; round++) {
        CHECK(queue.push(round));
        REQUIRE(queue.pop(value));
        CHECK(value == round);
    }
}

TEST_CASE("SpscQueue: un productor y un consumidor en hilos distintos no pierden ni repiten nada") {
    struct Item {
        std::uint32_t index;
        std::uint32_t check;
    };
    SpscQueue<Item, 64> queue;
    constexpr std::uint32_t kCount = 200000;
    std::thread producer([&] {
        for (std::uint32_t i = 0; i < kCount;)
            if (queue.push({i, i * 2654435761u})) i++;
    });
    std::uint32_t expected = 0;
    bool ordered = true;
    while (expected < kCount) {
        Item item{};
        if (!queue.pop(item)) continue;
        ordered = ordered && item.index == expected && item.check == expected * 2654435761u;
        expected++;
    }
    producer.join();
    CHECK(ordered);
    CHECK(queue.empty());
}
