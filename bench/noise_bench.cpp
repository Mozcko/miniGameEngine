// Benchmark de ruido (no es un test): ns por muestra de cada función. Compilar en Release.
// Uso: ./bench/noise_bench [muestras]   (por defecto 1000000)
#include <mge/procgen/Noise.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
    template <typename Fn> void measure(const char* name, int count, Fn&& fn) {
        volatile float sink = 0.0f; // Evita que el compilador elimine el trabajo
        auto start = std::chrono::steady_clock::now();
        float sum = 0.0f;
        for (int i = 0; i < count; i++) {
            const float x = static_cast<float>(i) * 0.0137f;
            sum += fn(x, x * 0.71f + 3.1f, x * 0.37f - 1.9f);
        }
        sink = sum;
        const double ns =
            std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - start).count();
        std::printf("  %-28s %7.2f ns/muestra\n", name, ns / count);
        (void)sink;
    }
}

int main(int argc, char** argv) {
    const int count = argc > 1 ? std::atoi(argv[1]) : 1000000;
    const mge::Noise noise(1337);
    mge::FractalSettings fractal; // 5 octavas
    std::printf("Noise benchmark (%d muestras)\n", count);
    measure("value 2D", count, [&](float x, float y, float) { return noise.value(x, y); });
    measure("perlin 2D", count, [&](float x, float y, float) { return noise.perlin(x, y); });
    measure("simplex 2D", count, [&](float x, float y, float) { return noise.simplex(x, y); });
    measure("worley 2D (f1)", count, [&](float x, float y, float) { return noise.worley(x, y).f1; });
    measure("value 3D", count, [&](float x, float y, float z) { return noise.value(x, y, z); });
    measure("perlin 3D", count, [&](float x, float y, float z) { return noise.perlin(x, y, z); });
    measure("simplex 3D", count, [&](float x, float y, float z) { return noise.simplex(x, y, z); });
    measure("worley 3D (f1)", count, [&](float x, float y, float z) { return noise.worley(x, y, z).f1; });
    measure("fbm simplex 2D (5 oct)", count,
            [&](float x, float y, float) { return noise.fbm(mge::NoiseType::Simplex, x, y, fractal); });
    measure("domainWarp simplex 2D", count, [&](float x, float y, float) {
        return noise.domainWarp(mge::NoiseType::Simplex, x, y, 1.5f, fractal);
    });
    return 0;
}
