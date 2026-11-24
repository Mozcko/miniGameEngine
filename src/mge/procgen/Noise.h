#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include <glm/glm.hpp>

namespace mge {

    enum class NoiseType {
        Value,
        Perlin,
        Simplex
    };

    // Fractal noise parameters (sum of octaves)
    struct FractalSettings {
        int octaves = 5;         // Detail layers
        float frequency = 1.0f;  // Scale of the first octave
        float lacunarity = 2.0f; // Frequency multiplier per octave
        float gain = 0.5f;       // Amplitude multiplier per octave (persistence)
    };

    // Worley (cellular noise) distances to the two nearest feature points
    struct WorleyResult {
        float f1; // Distance to nearest
        float f2; // Distance to second nearest
    };

    // Deterministic coherent noise. Same seed -> same values on every platform.
    // value/perlin/simplex return roughly [-1, 1] (clamped to that range).
    class Noise {
    public:
        explicit Noise(std::uint32_t seed = 0);

        std::uint32_t seed() const { return m_seed; }
        const std::array<std::uint8_t, 256>& permutationTable() const { return m_perm; }

        float value(float x, float y) const;
        float value(float x, float y, float z) const;
        float perlin(float x, float y) const; // Exactly 0 at integer coordinates
        float perlin(float x, float y, float z) const;
        float simplex(float x, float y) const;
        float simplex(float x, float y, float z) const;
        WorleyResult worley(float x, float y) const; // One feature point per cell
        WorleyResult worley(float x, float y, float z) const;

        float sample(NoiseType type, float x, float y) const;
        float sample(NoiseType type, float x, float y, float z) const;

        // --- Fractals (octaves normalized by the sum of amplitudes) ---
        float fbm(NoiseType type, float x, float y, const FractalSettings& settings = {}) const; // [-1, 1]
        float fbm(NoiseType type, float x, float y, float z, const FractalSettings& settings = {}) const;
        float ridged(NoiseType type, float x, float y,
                     const FractalSettings& settings = {}) const; // [0, 1], ridges
        float billow(NoiseType type, float x, float y,
                     const FractalSettings& settings = {}) const; // [-1, 1], puffy / clouds

        // Distorts coordinates with another fbm before sampling: more organic shapes
        float domainWarp(NoiseType type, float x, float y, float strength,
                         const FractalSettings& settings = {}) const;
        float domainWarp(NoiseType type, float x, float y, float z, float strength,
                         const FractalSettings& settings = {}) const;

        // --- Batch sampling (simple loops, compiler-vectorizable) ---
        void sample(NoiseType type, const float* xs, const float* ys, float* out, std::size_t count) const;
        void sample(NoiseType type, const float* xs, const float* ys, const float* zs, float* out,
                    std::size_t count) const;

    private:
        int hash(int x) const { return m_perm[static_cast<std::size_t>(x & 255)]; }
        int hash(int x, int y) const { return hash(x + hash(y)); }
        int hash(int x, int y, int z) const { return hash(x + hash(y + hash(z))); }

        std::uint32_t m_seed;
        std::array<std::uint8_t, 256> m_perm{};
    };
}
