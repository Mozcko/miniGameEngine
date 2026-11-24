#pragma once

#include <cstdint>

namespace mge {

    // --- Stateless mixers (64-bit) ---

    // SplitMix64 finalizer: thoroughly mixes all bits (avalanche). Stateless.
    constexpr std::uint64_t mix64(std::uint64_t x) {
        x += 0x9E3779B97F4A7C15ull;
        x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
        x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
        return x ^ (x >> 31);
    }

    // Stateful SplitMix64: useful for deriving multiple seeds from a single one
    constexpr std::uint64_t splitmix64(std::uint64_t& state) {
        std::uint64_t result = mix64(state);
        state += 0x9E3779B97F4A7C15ull;
        return result;
    }

    // --- Integer coordinate hash (stateless, deterministic across all platforms) ---
    // Same (x, z, seed) -> always the same value; neighbors produce uncorrelated values.
    constexpr std::uint32_t hash2i(std::int32_t x, std::int32_t z, std::uint32_t seed) {
        std::uint64_t packed =
            (static_cast<std::uint64_t>(static_cast<std::uint32_t>(x)) << 32) | static_cast<std::uint32_t>(z);
        return static_cast<std::uint32_t>(mix64(packed ^ mix64(seed)) >> 32);
    }

    constexpr std::uint32_t hash3i(std::int32_t x, std::int32_t y, std::int32_t z, std::uint32_t seed) {
        std::uint64_t h = mix64(static_cast<std::uint64_t>(static_cast<std::uint32_t>(x)) ^ mix64(seed));
        h = mix64(h ^ static_cast<std::uint32_t>(y));
        h = mix64(h ^ (static_cast<std::uint64_t>(static_cast<std::uint32_t>(z)) << 32));
        return static_cast<std::uint32_t>(h >> 32);
    }

    // u32 -> float in [0, 1) using the top 24 bits (exact in float)
    constexpr float toFloat01(std::uint32_t value) {
        return static_cast<float>(value >> 8) * (1.0f / 16777216.0f);
    }

    // --- PCG32 (O'Neill, pcg-random.org): fast, small, high-quality generator ---
    // Matches the reference "pcg32_srandom/pcg32_random", so it can be verified against its test vectors.
    class Pcg32 {
    public:
        explicit Pcg32(std::uint64_t seed = 0x853C49E6748FEA9Bull,
                       std::uint64_t stream = 0xDA3E39CB94B95BDBull) {
            reseed(seed, stream);
        }

        // Different streams with the same seed produce independent sequences
        void reseed(std::uint64_t seed, std::uint64_t stream = 0xDA3E39CB94B95BDBull) {
            m_state = 0u;
            m_increment = (stream << 1u) | 1u;
            nextU32();
            m_state += seed;
            nextU32();
        }

        std::uint32_t nextU32() {
            std::uint64_t old = m_state;
            m_state = old * 6364136223846793005ull + m_increment;
            std::uint32_t xorshifted = static_cast<std::uint32_t>(((old >> 18u) ^ old) >> 27u);
            std::uint32_t rot = static_cast<std::uint32_t>(old >> 59u);
            return (xorshifted >> rot) | (xorshifted << ((0u - rot) & 31u));
        }

        float nextFloat01() { return toFloat01(nextU32()); }

        // Uniform integer in [0, bound) without bias (rejection sampling, like pcg32_boundedrand). bound 0 -> 0.
        std::uint32_t nextBounded(std::uint32_t bound) {
            if (bound == 0) return 0;
            std::uint32_t threshold = (0u - bound) % bound;
            for (;;) {
                std::uint32_t r = nextU32();
                if (r >= threshold) return r % bound;
            }
        }

        // Uniform integer in [minInclusive, maxExclusive)
        std::int32_t range(std::int32_t minInclusive, std::int32_t maxExclusive) {
            if (maxExclusive <= minInclusive) return minInclusive;
            std::uint32_t span =
                static_cast<std::uint32_t>(static_cast<std::int64_t>(maxExclusive) - minInclusive);
            return static_cast<std::int32_t>(static_cast<std::int64_t>(minInclusive) + nextBounded(span));
        }

        // true with probability p (0..1)
        bool chance(float p) { return nextFloat01() < p; }

    private:
        std::uint64_t m_state = 0;
        std::uint64_t m_increment = 1;
    };
}
