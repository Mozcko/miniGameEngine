#include <doctest/doctest.h>
#include <mge/procgen/Noise.h>

#include <mge/core/Random.h>

#include <cmath>
#include <vector>

using namespace mge;

namespace {
    const NoiseType kTypes[] = {NoiseType::Value, NoiseType::Perlin, NoiseType::Simplex};

    // Puntos pseudoaleatorios en [-100, 100] (reproducibles)
    std::vector<glm::vec3> samplePoints(int count) {
        Pcg32 rng(99);
        std::vector<glm::vec3> points;
        for (int i = 0; i < count; i++) {
            points.push_back({rng.nextFloat01() * 200 - 100, rng.nextFloat01() * 200 - 100,
                              rng.nextFloat01() * 200 - 100});
        }
        return points;
    }
}

TEST_CASE("Determinismo: misma semilla mismos valores, otra semilla otros") {
    Noise a(42), b(42), c(43);
    int different = 0;
    for (const glm::vec3& p : samplePoints(200)) {
        for (NoiseType type : kTypes) {
            CHECK(a.sample(type, p.x, p.y) == b.sample(type, p.x, p.y));
            CHECK(a.sample(type, p.x, p.y, p.z) == b.sample(type, p.x, p.y, p.z));
            if (a.sample(type, p.x, p.y) != c.sample(type, p.x, p.y)) different++;
        }
        CHECK(a.worley(p.x, p.y).f1 == b.worley(p.x, p.y).f1);
    }
    CHECK(different > 500); // Casi todas difieren
}

TEST_CASE("Rango [-1, 1] y media cercana a 0 (2D y 3D)") {
    Noise noise(7);
    const auto points = samplePoints(20000);
    for (NoiseType type : kTypes) {
        CAPTURE(static_cast<int>(type));
        double sum2 = 0.0, sum3 = 0.0;
        float minV = 1.0f, maxV = -1.0f;
        for (const glm::vec3& p : points) {
            const float v2 = noise.sample(type, p.x, p.y);
            const float v3 = noise.sample(type, p.x, p.y, p.z);
            CHECK(v2 >= -1.0f);
            CHECK(v2 <= 1.0f);
            CHECK(v3 >= -1.0f);
            CHECK(v3 <= 1.0f);
            minV = std::min(minV, v2);
            maxV = std::max(maxV, v2);
            sum2 += v2;
            sum3 += v3;
        }
        CHECK(std::abs(sum2 / points.size()) < 0.05);
        CHECK(std::abs(sum3 / points.size()) < 0.05);
        CHECK(maxV - minV > 1.0f); // Usa buena parte del rango (no está "aplastado")
    }
}

TEST_CASE("Perlin vale 0 en los puntos enteros") {
    Noise noise(3);
    for (int x = -5; x <= 5; x++) {
        for (int y = -5; y <= 5; y++) {
            CHECK(noise.perlin(static_cast<float>(x), static_cast<float>(y)) == doctest::Approx(0.0f));
            CHECK(noise.perlin(static_cast<float>(x), static_cast<float>(y), 2.0f) == doctest::Approx(0.0f));
        }
    }
}

TEST_CASE("Continuidad: pasos pequeños dan cambios pequeños") {
    Noise noise(11);
    const float step = 1e-3f;
    for (const glm::vec3& p : samplePoints(2000)) {
        for (NoiseType type : kTypes) {
            const float a = noise.sample(type, p.x, p.y);
            const float b = noise.sample(type, p.x + step, p.y + step);
            CHECK(std::abs(a - b) < 0.02f); // Pendiente acotada (Lipschitz)
        }
    }
}

TEST_CASE("Worley: f1 <= f2, distancias acotadas y 0 cerca de un punto característico") {
    Noise noise(5);
    for (const glm::vec3& p : samplePoints(2000)) {
        WorleyResult r2 = noise.worley(p.x, p.y);
        WorleyResult r3 = noise.worley(p.x, p.y, p.z);
        CHECK(r2.f1 <= r2.f2);
        CHECK(r2.f1 < 1.5f); // Siempre hay un punto en la celda propia: distancia < √2
        CHECK(r3.f1 <= r3.f2);
        CHECK(r3.f1 < 1.8f); // < √3
    }
}

TEST_CASE("fbm, ridged y billow respetan sus rangos; más octavas agregan detalle") {
    Noise noise(21);
    FractalSettings one;
    one.octaves = 1;
    FractalSettings many;
    many.octaves = 6;
    // Detalle relativo: variación a escala fina dividida por la variación a escala gruesa.
    // fbm normaliza por la suma de amplitudes, así que hay que comparar proporciones, no valores.
    double fineOne = 0.0, coarseOne = 0.0, fineMany = 0.0, coarseMany = 0.0;
    for (const glm::vec3& p : samplePoints(5000)) {
        const float f = noise.fbm(NoiseType::Simplex, p.x, p.y, many);
        CHECK(f >= -1.0f);
        CHECK(f <= 1.0f);
        const float r = noise.ridged(NoiseType::Perlin, p.x, p.y, many);
        CHECK(r >= 0.0f);
        CHECK(r <= 1.0f);
        const float b = noise.billow(NoiseType::Value, p.x, p.y, many);
        CHECK(b >= -1.0f);
        CHECK(b <= 1.0f);
        CHECK(noise.fbm(NoiseType::Perlin, p.x, p.y, p.z, many) <= 1.0f);

        auto diff = [&](const FractalSettings& s, float step) {
            return std::abs(noise.fbm(NoiseType::Simplex, p.x, p.y, s) -
                            noise.fbm(NoiseType::Simplex, p.x + step, p.y, s));
        };
        fineOne += diff(one, 0.02f);
        coarseOne += diff(one, 1.0f);
        fineMany += diff(many, 0.02f);
        coarseMany += diff(many, 1.0f);
    }
    CHECK(fineMany / coarseMany > fineOne / coarseOne);

    // Una sola octava es exactamente el ruido base escalado por la frecuencia
    FractalSettings scaled;
    scaled.octaves = 1;
    scaled.frequency = 3.0f;
    CHECK(noise.fbm(NoiseType::Perlin, 1.3f, -2.7f, scaled) == noise.perlin(1.3f * 3.0f, -2.7f * 3.0f));
}

TEST_CASE("domainWarp: determinista y distinto del fbm sin deformar") {
    Noise noise(8);
    int different = 0;
    for (const glm::vec3& p : samplePoints(500)) {
        const float warped = noise.domainWarp(NoiseType::Simplex, p.x, p.y, 2.0f);
        CHECK(warped == noise.domainWarp(NoiseType::Simplex, p.x, p.y, 2.0f));
        CHECK(warped >= -1.0f);
        CHECK(warped <= 1.0f);
        if (warped != noise.fbm(NoiseType::Simplex, p.x, p.y)) different++;
        CHECK(noise.domainWarp(NoiseType::Perlin, p.x, p.y, p.z, 1.0f) <= 1.0f);
    }
    CHECK(different > 450);
}

TEST_CASE("El muestreo por lotes da lo mismo que punto a punto") {
    Noise noise(13);
    const auto points = samplePoints(256);
    std::vector<float> xs, ys, zs, out2(points.size()), out3(points.size());
    for (const glm::vec3& p : points) {
        xs.push_back(p.x);
        ys.push_back(p.y);
        zs.push_back(p.z);
    }
    for (NoiseType type : kTypes) {
        noise.sample(type, xs.data(), ys.data(), out2.data(), points.size());
        noise.sample(type, xs.data(), ys.data(), zs.data(), out3.data(), points.size());
        for (std::size_t i = 0; i < points.size(); i++) {
            CHECK(out2[i] == noise.sample(type, xs[i], ys[i]));
            CHECK(out3[i] == noise.sample(type, xs[i], ys[i], zs[i]));
        }
    }
}
