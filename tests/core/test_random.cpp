#include <doctest/doctest.h>
#include <mge/core/Random.h>

#include <array>
#include <cstdint>
#include <set>

using namespace mge;

namespace {
    // Chi-square of bucket counts against a uniform distribution
    template <std::size_t N> double chiSquare(const std::array<int, N>& counts, int total) {
        const double expected = static_cast<double>(total) / N;
        double chi = 0.0;
        for (int c : counts) chi += (c - expected) * (c - expected) / expected;
        return chi;
    }
}

TEST_CASE("PCG32 reproduces reference vectors (pcg32_srandom(42, 54))") {
    // Output from the official pcg32-demo at pcg-random.org
    Pcg32 rng(42u, 54u);
    const std::uint32_t expected[] = {0xa15c02b7, 0x7b47f409, 0xba1d3330, 0x83d2f293, 0xbfa4784b, 0xcbed606e};
    for (std::uint32_t value : expected) CHECK(rng.nextU32() == value);
}

TEST_CASE("Same seed and stream -> same sequence; different stream -> different sequence") {
    Pcg32 a(1234, 1), b(1234, 1), c(1234, 2);
    bool anyDifferent = false;
    for (int i = 0; i < 100; i++) {
        std::uint32_t va = a.nextU32();
        CHECK(va == b.nextU32());
        if (va != c.nextU32()) anyDifferent = true;
    }
    CHECK(anyDifferent);
}

TEST_CASE("range and nextFloat01 respect their bounds") {
    Pcg32 rng(7);
    for (int i = 0; i < 10000; i++) {
        int v = rng.range(-3, 4);
        CHECK(v >= -3);
        CHECK(v < 4);
        float f = rng.nextFloat01();
        CHECK(f >= 0.0f);
        CHECK(f < 1.0f);
    }
    CHECK(rng.range(5, 5) == 5);                        // Empty range
    CHECK(rng.nextBounded(0) == 0u);                    // Bound 0
    CHECK(rng.range(INT32_MIN, INT32_MAX) < INT32_MAX); // No overflow
}

TEST_CASE("range is approximately uniform (chi-square)") {
    Pcg32 rng(2024);
    std::array<int, 10> counts{};
    const int total = 100000;
    for (int i = 0; i < total; i++) counts[static_cast<std::size_t>(rng.range(0, 10))]++;
    // 9 degrees of freedom: 27.9 corresponds to p = 0.001 (very loose, avoids false positives)
    CHECK(chiSquare(counts, total) < 27.9);
}

TEST_CASE("hash2i: deterministic, sensitive to each coordinate and seed") {
    CHECK(hash2i(10, -3, 99) == hash2i(10, -3, 99));
    CHECK(hash2i(10, -3, 99) != hash2i(11, -3, 99));
    CHECK(hash2i(10, -3, 99) != hash2i(10, -2, 99));
    CHECK(hash2i(10, -3, 99) != hash2i(-3, 10, 99)); // Not symmetric
    CHECK(hash2i(10, -3, 99) != hash2i(10, -3, 100));
    // Huge coordinates (previously overflowed an int with srand)
    CHECK(hash2i(INT32_MAX, INT32_MIN, 1) != hash2i(INT32_MIN, INT32_MAX, 1));
}

TEST_CASE("hash2i over a grid: no visible collisions and uniform high bits") {
    std::set<std::uint32_t> seen;
    std::array<int, 16> counts{};
    int total = 0;
    for (int x = -50; x < 50; x++) {
        for (int z = -50; z < 50; z++) {
            std::uint32_t h = hash2i(x, z, 1337);
            seen.insert(h);
            counts[h >> 28]++;
            total++;
        }
    }
    CHECK(seen.size() == static_cast<std::size_t>(total)); // 10,000 distinct values
    CHECK(chiSquare(counts, total) < 37.7);                // 15 d.f., p = 0.001
}

TEST_CASE("hash3i and toFloat01") {
    CHECK(hash3i(1, 2, 3, 0) == hash3i(1, 2, 3, 0));
    CHECK(hash3i(1, 2, 3, 0) != hash3i(1, 3, 2, 0));
    CHECK(toFloat01(0u) == doctest::Approx(0.0f));
    CHECK(toFloat01(0xFFFFFFFFu) < 1.0f);
}

TEST_CASE("splitmix64 advances state and mix64 is constexpr") {
    std::uint64_t state = 0;
    std::uint64_t a = splitmix64(state);
    std::uint64_t b = splitmix64(state);
    CHECK(a != b);
    static_assert(mix64(1) != mix64(2), "mix64 can be evaluated at compile time");
}
