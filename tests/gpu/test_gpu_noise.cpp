#include "GpuTestContext.h"

#include <mge/gpu/GpuNoiseField.h>
#include <mge/procgen/Noise.h>

#include <algorithm>
#include <cmath>
#include <vector>

using namespace mge;
using mge_test::GpuTestContext;

TEST_CASE("GPU noise: simplex 3D en rejilla 32^3 coincide con CPU mge::Noise (< 1e-4)") {
    GpuTestContext gpu;
    if (!gpu.available()) return;

    constexpr std::uint32_t kSize = 32;
    constexpr std::uint32_t kSeed = 1337;
    const Noise cpuNoise(kSeed);

    GpuNoiseField field(gpu.device(), cpuNoise);

    GpuNoiseFieldDesc desc;
    desc.width = kSize;
    desc.height = kSize;
    desc.depth = kSize;
    desc.origin = glm::vec3(0.0f);
    desc.scale = glm::vec3(0.1f);
    desc.fractal.octaves = 1;
    desc.fractal.frequency = 1.0f;

    field.generate(desc);
    const std::vector<float> gpuValues = field.download();
    REQUIRE(gpuValues.size() == kSize * kSize * kSize);

    float maxDiff = 0.0f;
    glm::uvec3 worstCoord{0};
    float worstExpected = 0.0f;
    float worstActual = 0.0f;
    for (std::uint32_t z = 0; z < kSize; ++z) {
        for (std::uint32_t y = 0; y < kSize; ++y) {
            for (std::uint32_t x = 0; x < kSize; ++x) {
                const glm::vec3 pos = desc.origin + glm::vec3(x, y, z) * desc.scale;
                const float expected = cpuNoise.simplex(pos.x, pos.y, pos.z);
                const float actual = gpuValues[field.index(x, y, z)];
                const float diff = std::abs(actual - expected);
                if (diff > maxDiff) {
                    maxDiff = diff;
                    worstCoord = {x, y, z};
                    worstExpected = expected;
                    worstActual = actual;
                }
            }
        }
    }
    INFO("Worst diff at (", worstCoord.x, ", ", worstCoord.y, ", ", worstCoord.z,
         "): expected=", worstExpected, ", actual=", worstActual, ", diff=", maxDiff);
    CHECK(maxDiff < 1e-4f);
}

TEST_CASE("GPU noise: fBm 3D (5 octavas) en rejilla 32^3 coincide con CPU mge::Noise (< 1e-4)") {
    GpuTestContext gpu;
    if (!gpu.available()) return;

    constexpr std::uint32_t kSize = 32;
    constexpr std::uint32_t kSeed = 42;
    const Noise cpuNoise(kSeed);

    GpuNoiseField field(gpu.device(), cpuNoise);

    GpuNoiseFieldDesc desc;
    desc.width = kSize;
    desc.height = kSize;
    desc.depth = kSize;
    desc.origin = glm::vec3(15.5f, -7.25f, 3.1415f);
    desc.scale = glm::vec3(0.08f, 0.12f, 0.05f);
    desc.fractal.octaves = 5;
    desc.fractal.frequency = 1.0f;
    desc.fractal.lacunarity = 2.0f;
    desc.fractal.gain = 0.5f;

    field.generate(desc);
    const std::vector<float> gpuValues = field.download();
    REQUIRE(gpuValues.size() == kSize * kSize * kSize);

    float maxDiff = 0.0f;
    glm::uvec3 worstCoord{0};
    float worstExpected = 0.0f;
    float worstActual = 0.0f;
    for (std::uint32_t z = 0; z < kSize; ++z) {
        for (std::uint32_t y = 0; y < kSize; ++y) {
            for (std::uint32_t x = 0; x < kSize; ++x) {
                const glm::vec3 pos = desc.origin + glm::vec3(x, y, z) * desc.scale;
                const float expected = cpuNoise.fbm(NoiseType::Simplex, pos.x, pos.y, pos.z, desc.fractal);
                const float actual = gpuValues[field.index(x, y, z)];
                const float diff = std::abs(actual - expected);
                if (diff > maxDiff) {
                    maxDiff = diff;
                    worstCoord = {x, y, z};
                    worstExpected = expected;
                    worstActual = actual;
                }
            }
        }
    }
    INFO("Worst diff at (", worstCoord.x, ", ", worstCoord.y, ", ", worstCoord.z,
         "): expected=", worstExpected, ", actual=", worstActual, ", diff=", maxDiff);
    CHECK(maxDiff < 1e-4f);
}

TEST_CASE("GPU noise: cambio de semilla y regeneración") {
    GpuTestContext gpu;
    if (!gpu.available()) return;

    GpuNoiseField field(gpu.device(), 111);
    GpuNoiseFieldDesc desc;
    desc.width = 16;
    desc.height = 16;
    desc.depth = 16;
    desc.origin = glm::vec3(1.0f);
    desc.scale = glm::vec3(0.2f);
    desc.fractal.octaves = 3;

    field.generate(desc);
    const std::vector<float> v1 = field.download();

    // Cambiar a semilla distinta
    field.setSeed(222);
    field.generate(desc);
    const std::vector<float> v2 = field.download();

    REQUIRE(v1.size() == v2.size());
    bool different = false;
    for (std::size_t i = 0; i < v1.size(); ++i) {
        if (std::abs(v1[i] - v2[i]) > 0.01f) {
            different = true;
            break;
        }
    }
    CHECK(different);

    // Comparar v2 con CPU Noise(222)
    const Noise cpuNoise(222);
    float maxDiff = 0.0f;
    glm::uvec3 worstCoord{0};
    float worstExpected = 0.0f;
    float worstActual = 0.0f;
    for (std::uint32_t z = 0; z < 16; ++z) {
        for (std::uint32_t y = 0; y < 16; ++y) {
            for (std::uint32_t x = 0; x < 16; ++x) {
                const glm::vec3 pos = desc.origin + glm::vec3(x, y, z) * desc.scale;
                const float expected = cpuNoise.fbm(NoiseType::Simplex, pos.x, pos.y, pos.z, desc.fractal);
                const float actual = v2[field.index(x, y, z)];
                const float diff = std::abs(actual - expected);
                if (diff > maxDiff) {
                    maxDiff = diff;
                    worstCoord = {x, y, z};
                    worstExpected = expected;
                    worstActual = actual;
                }
            }
        }
    }
    INFO("Worst diff at (", worstCoord.x, ", ", worstCoord.y, ", ", worstCoord.z,
         "): expected=", worstExpected, ", actual=", worstActual, ", diff=", maxDiff);
    CHECK(maxDiff < 1e-4f);
}
