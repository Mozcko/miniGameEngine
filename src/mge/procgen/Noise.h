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

    // Parámetros de ruido fractal (suma de octavas)
    struct FractalSettings {
        int octaves = 5;         // Capas de detalle
        float frequency = 1.0f;  // Escala de la primera octava
        float lacunarity = 2.0f; // Multiplicador de frecuencia por octava
        float gain = 0.5f;       // Multiplicador de amplitud por octava (persistencia)
    };

    // Distancias de Worley (ruido celular) a los dos puntos característicos más cercanos
    struct WorleyResult {
        float f1; // Distancia al más cercano
        float f2; // Distancia al segundo
    };

    // Ruido coherente determinista. Mismo seed -> mismos valores en cualquier plataforma.
    // value/perlin/simplex devuelven aproximadamente [-1, 1] (recortado a ese rango).
    class Noise {
    public:
        explicit Noise(std::uint32_t seed = 0);

        std::uint32_t seed() const { return m_seed; }

        float value(float x, float y) const;
        float value(float x, float y, float z) const;
        float perlin(float x, float y) const; // 0 exacto en los puntos enteros
        float perlin(float x, float y, float z) const;
        float simplex(float x, float y) const;
        float simplex(float x, float y, float z) const;
        WorleyResult worley(float x, float y) const; // Un punto característico por celda
        WorleyResult worley(float x, float y, float z) const;

        float sample(NoiseType type, float x, float y) const;
        float sample(NoiseType type, float x, float y, float z) const;

        // --- Fractales (octavas normalizadas por la suma de amplitudes) ---
        float fbm(NoiseType type, float x, float y, const FractalSettings& settings = {}) const; // [-1, 1]
        float fbm(NoiseType type, float x, float y, float z, const FractalSettings& settings = {}) const;
        float ridged(NoiseType type, float x, float y,
                     const FractalSettings& settings = {}) const; // [0, 1], crestas
        float billow(NoiseType type, float x, float y,
                     const FractalSettings& settings = {}) const; // [-1, 1], "nubes"

        // Distorsiona las coordenadas con otro fbm antes de muestrear: formas más orgánicas
        float domainWarp(NoiseType type, float x, float y, float strength,
                         const FractalSettings& settings = {}) const;
        float domainWarp(NoiseType type, float x, float y, float z, float strength,
                         const FractalSettings& settings = {}) const;

        // --- Por lotes (bucles simples, vectorizables por el compilador) ---
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
