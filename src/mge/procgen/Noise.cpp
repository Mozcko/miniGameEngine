#include <mge/procgen/Noise.h>

#include <mge/core/Random.h>

#include <algorithm>
#include <cmath>

namespace mge {

    namespace {
        float fade(float t) {
            return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
        } // Quíntica de Perlin
        float lerp(float a, float b, float t) {
            return a + (b - a) * t;
        }
        int fastFloor(float v) {
            return static_cast<int>(std::floor(v));
        }
        float clamp1(float v) {
            return std::clamp(v, -1.0f, 1.0f);
        }
        float lattice(int h) {
            return static_cast<float>(h) / 127.5f - 1.0f;
        } // [0,255] -> [-1, 1]

        // 8 direcciones unitarias para Perlin 2D
        const glm::vec2 kGrad2[8] = {{1, 0},
                                     {-1, 0},
                                     {0, 1},
                                     {0, -1},
                                     {0.70710678f, 0.70710678f},
                                     {-0.70710678f, 0.70710678f},
                                     {0.70710678f, -0.70710678f},
                                     {-0.70710678f, -0.70710678f}};
        // 12 aristas del cubo para Perlin/simplex 3D (Perlin mejorado)
        const glm::vec3 kGrad3[12] = {{1, 1, 0}, {-1, 1, 0}, {1, -1, 0}, {-1, -1, 0},
                                      {1, 0, 1}, {-1, 0, 1}, {1, 0, -1}, {-1, 0, -1},
                                      {0, 1, 1}, {0, -1, 1}, {0, 1, -1}, {0, -1, -1}};
    }

    Noise::Noise(std::uint32_t seed) : m_seed(seed) {
        for (std::size_t i = 0; i < 256; i++) m_perm[i] = static_cast<std::uint8_t>(i);
        // Fisher–Yates con el RNG del motor: tabla de permutación distinta por semilla
        Pcg32 rng(seed, 0x6E6F697365ull); // stream "noise"
        for (std::size_t i = 255; i > 0; i--) {
            std::size_t j = rng.nextBounded(static_cast<std::uint32_t>(i + 1));
            std::swap(m_perm[i], m_perm[j]);
        }
    }

    // --- Value ---

    float Noise::value(float x, float y) const {
        const int x0 = fastFloor(x), y0 = fastFloor(y);
        const float tx = fade(x - static_cast<float>(x0)), ty = fade(y - static_cast<float>(y0));
        const float a = lattice(hash(x0, y0)), b = lattice(hash(x0 + 1, y0));
        const float c = lattice(hash(x0, y0 + 1)), d = lattice(hash(x0 + 1, y0 + 1));
        return lerp(lerp(a, b, tx), lerp(c, d, tx), ty);
    }

    float Noise::value(float x, float y, float z) const {
        const int x0 = fastFloor(x), y0 = fastFloor(y), z0 = fastFloor(z);
        const float tx = fade(x - static_cast<float>(x0)), ty = fade(y - static_cast<float>(y0)),
                    tz = fade(z - static_cast<float>(z0));
        auto v = [&](int dx, int dy, int dz) { return lattice(hash(x0 + dx, y0 + dy, z0 + dz)); };
        const float front = lerp(lerp(v(0, 0, 0), v(1, 0, 0), tx), lerp(v(0, 1, 0), v(1, 1, 0), tx), ty);
        const float back = lerp(lerp(v(0, 0, 1), v(1, 0, 1), tx), lerp(v(0, 1, 1), v(1, 1, 1), tx), ty);
        return lerp(front, back, tz);
    }

    // --- Perlin ---

    float Noise::perlin(float x, float y) const {
        const int x0 = fastFloor(x), y0 = fastFloor(y);
        const float fx = x - static_cast<float>(x0), fy = y - static_cast<float>(y0);
        auto corner = [&](int dx, int dy) {
            const glm::vec2& g = kGrad2[hash(x0 + dx, y0 + dy) & 7];
            return g.x * (fx - static_cast<float>(dx)) + g.y * (fy - static_cast<float>(dy));
        };
        const float u = fade(fx), v = fade(fy);
        const float result =
            lerp(lerp(corner(0, 0), corner(1, 0), u), lerp(corner(0, 1), corner(1, 1), u), v);
        return clamp1(result * 1.41421356f); // Rango teórico ±√2/2 -> ±1
    }

    float Noise::perlin(float x, float y, float z) const {
        const int x0 = fastFloor(x), y0 = fastFloor(y), z0 = fastFloor(z);
        const float fx = x - static_cast<float>(x0), fy = y - static_cast<float>(y0),
                    fz = z - static_cast<float>(z0);
        auto corner = [&](int dx, int dy, int dz) {
            const glm::vec3& g = kGrad3[hash(x0 + dx, y0 + dy, z0 + dz) % 12];
            return glm::dot(g, glm::vec3(fx - static_cast<float>(dx), fy - static_cast<float>(dy),
                                         fz - static_cast<float>(dz)));
        };
        const float u = fade(fx), v = fade(fy), w = fade(fz);
        const float front =
            lerp(lerp(corner(0, 0, 0), corner(1, 0, 0), u), lerp(corner(0, 1, 0), corner(1, 1, 0), u), v);
        const float back =
            lerp(lerp(corner(0, 0, 1), corner(1, 0, 1), u), lerp(corner(0, 1, 1), corner(1, 1, 1), u), v);
        return clamp1(lerp(front, back, w));
    }

    // --- Simplex (Gustavson) ---

    float Noise::simplex(float x, float y) const {
        constexpr float kF2 = 0.36602540378f; // (√3 - 1) / 2
        constexpr float kG2 = 0.21132486540f; // (3 - √3) / 6
        const float s = (x + y) * kF2;
        const int i = fastFloor(x + s), j = fastFloor(y + s);
        const float t = static_cast<float>(i + j) * kG2;
        const float x0 = x - (static_cast<float>(i) - t), y0 = y - (static_cast<float>(j) - t);
        const int i1 = x0 > y0 ? 1 : 0, j1 = x0 > y0 ? 0 : 1;
        const float x1 = x0 - static_cast<float>(i1) + kG2, y1 = y0 - static_cast<float>(j1) + kG2;
        const float x2 = x0 - 1.0f + 2.0f * kG2, y2 = y0 - 1.0f + 2.0f * kG2;
        auto contribution = [&](float dx, float dy, int gi) {
            float falloff = 0.5f - dx * dx - dy * dy;
            if (falloff < 0.0f) return 0.0f;
            falloff *= falloff;
            const glm::vec2& g = kGrad2[gi & 7];
            return falloff * falloff * (g.x * dx + g.y * dy);
        };
        const float n = contribution(x0, y0, hash(i, j)) + contribution(x1, y1, hash(i + i1, j + j1)) +
                        contribution(x2, y2, hash(i + 1, j + 1));
        return clamp1(70.0f * n);
    }

    float Noise::simplex(float x, float y, float z) const {
        constexpr float kF3 = 1.0f / 3.0f;
        constexpr float kG3 = 1.0f / 6.0f;
        const float s = (x + y + z) * kF3;
        const int i = fastFloor(x + s), j = fastFloor(y + s), k = fastFloor(z + s);
        const float t = static_cast<float>(i + j + k) * kG3;
        const glm::vec3 p0(x - (static_cast<float>(i) - t), y - (static_cast<float>(j) - t),
                           z - (static_cast<float>(k) - t));
        glm::ivec3 o1, o2;
        if (p0.x >= p0.y) {
            if (p0.y >= p0.z) {
                o1 = {1, 0, 0};
                o2 = {1, 1, 0};
            } else if (p0.x >= p0.z) {
                o1 = {1, 0, 0};
                o2 = {1, 0, 1};
            } else {
                o1 = {0, 0, 1};
                o2 = {1, 0, 1};
            }
        } else {
            if (p0.y < p0.z) {
                o1 = {0, 0, 1};
                o2 = {0, 1, 1};
            } else if (p0.x < p0.z) {
                o1 = {0, 1, 0};
                o2 = {0, 1, 1};
            } else {
                o1 = {0, 1, 0};
                o2 = {1, 1, 0};
            }
        }
        const glm::vec3 p1 = p0 - glm::vec3(o1) + kG3;
        const glm::vec3 p2 = p0 - glm::vec3(o2) + 2.0f * kG3;
        const glm::vec3 p3 = p0 - 1.0f + 3.0f * kG3;
        auto contribution = [&](const glm::vec3& d, int gi) {
            float falloff = 0.6f - glm::dot(d, d);
            if (falloff < 0.0f) return 0.0f;
            falloff *= falloff;
            return falloff * falloff * glm::dot(kGrad3[gi % 12], d);
        };
        const float n = contribution(p0, hash(i, j, k)) +
                        contribution(p1, hash(i + o1.x, j + o1.y, k + o1.z)) +
                        contribution(p2, hash(i + o2.x, j + o2.y, k + o2.z)) +
                        contribution(p3, hash(i + 1, j + 1, k + 1));
        return clamp1(32.0f * n);
    }

    // --- Worley ---

    WorleyResult Noise::worley(float x, float y) const {
        const int cx = fastFloor(x), cy = fastFloor(y);
        float f1 = 1e9f, f2 = 1e9f;
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                const int gx = cx + dx, gy = cy + dy;
                // Punto característico de la celda, determinista por (celda, semilla)
                const std::uint32_t h = hash2i(gx, gy, m_seed);
                const glm::vec2 feature(static_cast<float>(gx) + toFloat01(h),
                                        static_cast<float>(gy) +
                                            toFloat01(hash2i(gx, gy, m_seed ^ 0x9E3779B9u)));
                const float d = glm::length(feature - glm::vec2(x, y));
                if (d < f1) {
                    f2 = f1;
                    f1 = d;
                } else if (d < f2) {
                    f2 = d;
                }
            }
        }
        return {f1, f2};
    }

    WorleyResult Noise::worley(float x, float y, float z) const {
        const int cx = fastFloor(x), cy = fastFloor(y), cz = fastFloor(z);
        float f1 = 1e9f, f2 = 1e9f;
        for (int dz = -1; dz <= 1; dz++) {
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    const int gx = cx + dx, gy = cy + dy, gz = cz + dz;
                    const glm::vec3 feature(
                        static_cast<float>(gx) + toFloat01(hash3i(gx, gy, gz, m_seed)),
                        static_cast<float>(gy) + toFloat01(hash3i(gx, gy, gz, m_seed ^ 0x9E3779B9u)),
                        static_cast<float>(gz) + toFloat01(hash3i(gx, gy, gz, m_seed ^ 0x85EBCA6Bu)));
                    const float d = glm::length(feature - glm::vec3(x, y, z));
                    if (d < f1) {
                        f2 = f1;
                        f1 = d;
                    } else if (d < f2) {
                        f2 = d;
                    }
                }
            }
        }
        return {f1, f2};
    }

    // --- Muestreo genérico ---

    float Noise::sample(NoiseType type, float x, float y) const {
        switch (type) {
        case NoiseType::Value: return value(x, y);
        case NoiseType::Perlin: return perlin(x, y);
        case NoiseType::Simplex: return simplex(x, y);
        }
        return 0.0f;
    }

    float Noise::sample(NoiseType type, float x, float y, float z) const {
        switch (type) {
        case NoiseType::Value: return value(x, y, z);
        case NoiseType::Perlin: return perlin(x, y, z);
        case NoiseType::Simplex: return simplex(x, y, z);
        }
        return 0.0f;
    }

    // --- Fractales ---

    namespace {
        template <typename SampleFn> float fractal(const FractalSettings& s, SampleFn&& sampleAt) {
            float sum = 0.0f, amplitude = 1.0f, norm = 0.0f, frequency = s.frequency;
            for (int octave = 0; octave < std::max(s.octaves, 1); octave++) {
                sum += sampleAt(frequency, octave) * amplitude;
                norm += amplitude;
                amplitude *= s.gain;
                frequency *= s.lacunarity;
            }
            return norm > 0.0f ? sum / norm : 0.0f;
        }
        // Desplazamiento por octava para que las octavas no se alineen en el origen
        float octaveOffset(int octave) {
            return static_cast<float>(octave) * 17.31f;
        }
    }

    float Noise::fbm(NoiseType type, float x, float y, const FractalSettings& settings) const {
        return fractal(settings,
                       [&](float f, int o) { return sample(type, x * f + octaveOffset(o), y * f); });
    }

    float Noise::fbm(NoiseType type, float x, float y, float z, const FractalSettings& settings) const {
        return fractal(settings,
                       [&](float f, int o) { return sample(type, x * f + octaveOffset(o), y * f, z * f); });
    }

    float Noise::ridged(NoiseType type, float x, float y, const FractalSettings& settings) const {
        return fractal(settings, [&](float f, int o) {
            const float r = 1.0f - std::abs(sample(type, x * f + octaveOffset(o), y * f));
            return r * r;
        });
    }

    float Noise::billow(NoiseType type, float x, float y, const FractalSettings& settings) const {
        return fractal(settings, [&](float f, int o) {
            return std::abs(sample(type, x * f + octaveOffset(o), y * f)) * 2.0f - 1.0f;
        });
    }

    float Noise::domainWarp(NoiseType type, float x, float y, float strength,
                            const FractalSettings& settings) const {
        const float wx = fbm(type, x + 5.2f, y + 1.3f, settings);
        const float wy = fbm(type, x - 1.7f, y + 9.2f, settings);
        return fbm(type, x + wx * strength, y + wy * strength, settings);
    }

    float Noise::domainWarp(NoiseType type, float x, float y, float z, float strength,
                            const FractalSettings& settings) const {
        const float wx = fbm(type, x + 5.2f, y + 1.3f, z - 3.1f, settings);
        const float wy = fbm(type, x - 1.7f, y + 9.2f, z + 2.8f, settings);
        const float wz = fbm(type, x + 8.3f, y - 2.8f, z + 7.1f, settings);
        return fbm(type, x + wx * strength, y + wy * strength, z + wz * strength, settings);
    }

    // --- Lotes ---

    void Noise::sample(NoiseType type, const float* xs, const float* ys, float* out,
                       std::size_t count) const {
        for (std::size_t i = 0; i < count; i++) out[i] = sample(type, xs[i], ys[i]);
    }

    void Noise::sample(NoiseType type, const float* xs, const float* ys, const float* zs, float* out,
                       std::size_t count) const {
        for (std::size_t i = 0; i < count; i++) out[i] = sample(type, xs[i], ys[i], zs[i]);
    }
}
