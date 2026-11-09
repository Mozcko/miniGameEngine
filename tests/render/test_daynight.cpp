#include "../gpu/GpuTestContext.h"
#include "RenderTest.h"

#include <mge/graphics/DayNight.h>
#include <mge/graphics/MeshRenderer.h>
#include <mge/graphics/Sky.h>

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

using namespace mge;
using mge_test::GpuTestContext;

namespace {
    constexpr std::uint32_t kSize = 128;

    void checkGolden(const std::string& name, const mge_test::Image& image) {
        REQUIRE(image.rgba.size() == std::size_t(image.width) * image.height * 4);
        const mge_test::GoldenResult result = mge_test::compareWithGolden(name, image);
        if (!result.message.empty()) MESSAGE(result.message);
        CHECK(result.passed);
    }
}

// --- Tests headless de DayNight (pura matemática, sin GPU) ---

TEST_CASE("DayNight: hour wraps") {
    DayNight dn;
    dn.hour = 10.0f;
    dn.dayLengthSeconds = 24.0f; // 1 segundo = 1 hora

    dn.advance(0.0f);
    CHECK(dn.hour == doctest::Approx(10.0f));

    dn.advance(15.0f); // 10 + 15 = 25 -> 1.0f
    CHECK(dn.hour == doctest::Approx(1.0f));

    dn.advance(23.0f); // 1 + 23 = 24 -> 0.0f
    CHECK(dn.hour == doctest::Approx(0.0f));

    dn.advance(-2.0f); // 0 - 2 = -2 -> 22.0f
    CHECK(dn.hour == doctest::Approx(22.0f));

    dn.hour = 24.0f;
    dn.advance(0.0f);
    CHECK(dn.hour == doctest::Approx(0.0f));
}

TEST_CASE("DayNight: noon sun points down (sunDirection.y < -0.9)") {
    DayNight dn;
    dn.hour = 12.0f;
    const SkySettings sky = dn.sky();
    CHECK(sky.sunDirection.y < -0.9f);
}

TEST_CASE("DayNight: midnight sun below the horizon and starsIntensity > 0") {
    DayNight dn;
    dn.hour = 0.0f;
    const SkySettings sky = dn.sky();

    // El sol viaja hacia sunDirection, por lo que su posición en el cielo es -sunDirection.
    // Si está bajo el horizonte, la componente Y de su posición es negativa (< 0),
    // lo que equivale a sunDirection.y > 0.
    CHECK(sky.sunDirection.y > 0.0f);
    CHECK(sky.starsIntensity > 0.0f);
}

TEST_CASE("DayNight: fogColor == horizonColor at several hours") {
    DayNight dn;
    Lighting base;
    base.fog = true;
    base.fogStart = 50.0f;
    base.fogEnd = 120.0f;

    const float testHours[] = {0.0f, 4.0f, 5.5f, 6.0f, 8.0f, 12.0f, 17.5f, 18.0f, 20.0f, 23.5f};
    for (float h : testHours) {
        dn.hour = h;
        const SkySettings sky = dn.sky();
        const Lighting l = dn.lighting(base);
        CHECK(l.fogColor.r == doctest::Approx(sky.horizonColor.r));
        CHECK(l.fogColor.g == doctest::Approx(sky.horizonColor.g));
        CHECK(l.fogColor.b == doctest::Approx(sky.horizonColor.b));
    }
}

TEST_CASE("DayNight: colours continuous (no jump > 0.1 between hour h and h+0.01)") {
    DayNight dn1;
    DayNight dn2;

    auto maxComponentDiff = [](const glm::vec3& a, const glm::vec3& b) {
        return std::max({std::abs(a.r - b.r), std::abs(a.g - b.g), std::abs(a.b - b.b)});
    };

    for (float h = 0.0f; h < 24.0f; h += 0.05f) {
        dn1.hour = h;
        dn2.hour = h + 0.01f;
        const SkySettings s1 = dn1.sky();
        const SkySettings s2 = dn2.sky();

        CHECK(maxComponentDiff(s1.zenithColor, s2.zenithColor) <= 0.1f);
        CHECK(maxComponentDiff(s1.horizonColor, s2.horizonColor) <= 0.1f);
        CHECK(maxComponentDiff(s1.groundColor, s2.groundColor) <= 0.1f);
        CHECK(maxComponentDiff(s1.sunColor, s2.sunColor) <= 0.1f);
        CHECK(std::abs(s1.starsIntensity - s2.starsIntensity) <= 0.1f);

        const Lighting l1 = dn1.lighting({});
        const Lighting l2 = dn2.lighting({});
        CHECK(maxComponentDiff(l1.fogColor, l2.fogColor) <= 0.1f);
        CHECK(maxComponentDiff(l1.skyAmbient, l2.skyAmbient) <= 0.1f);
        CHECK(maxComponentDiff(l1.groundAmbient, l2.groundAmbient) <= 0.1f);
    }
}

// --- Render Golden Tests (requieren GPU offscreen) ---

TEST_CASE("Render: cielo al mediodía mirando al horizonte") {
    GpuTestContext gpu;
    if (!gpu.available()) return;
    GpuDevice& device = gpu.device();

    MeshRenderer renderer(device, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM);
    REQUIRE(renderer.valid());
    SkyRenderer skyRenderer(device, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, device.depthFormat());
    REQUIRE(skyRenderer.valid());

    DayNight dn;
    dn.hour = 12.0f; // Mediodía
    const SkySettings sky = dn.sky();

    const glm::vec3 cameraPos(0.0f);
    const glm::mat4 view = glm::lookAt(cameraPos, glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::mat4 proj = glm::perspectiveRH_ZO(glm::radians(60.0f), 1.0f, 0.1f, 1000.0f);
    const glm::mat4 viewProj = proj * view;
    const glm::mat4 invViewProj = glm::inverse(viewProj);

    GpuTexture target = device.createRenderTarget(kSize, kSize);
    CommandBuffer cmd = device.acquireCommandBuffer();
    if (renderer.begin(cmd, target.get(), kSize, kSize, glm::vec4(sky.horizonColor, 1.0f))) {
        renderer.setCamera(viewProj, cameraPos);
        skyRenderer.draw(renderer, invViewProj, cameraPos, sky);
        renderer.end();
    }
    REQUIRE(cmd.submitAndWait());

    const mge_test::Image image{kSize, kSize, device.downloadTexture(target, kSize, kSize)};
    checkGolden("sky_noon_horizon", image);
}

TEST_CASE("Render: cielo al atardecer mirando al sol") {
    GpuTestContext gpu;
    if (!gpu.available()) return;
    GpuDevice& device = gpu.device();

    MeshRenderer renderer(device, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM);
    REQUIRE(renderer.valid());
    SkyRenderer skyRenderer(device, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, device.depthFormat());
    REQUIRE(skyRenderer.valid());

    DayNight dn;
    dn.hour = 18.0f; // Puesta de sol
    const SkySettings sky = dn.sky();

    const glm::vec3 sunDirInSky = -glm::normalize(sky.sunDirection);
    const glm::vec3 cameraPos(0.0f);
    const glm::mat4 view = glm::lookAt(cameraPos, sunDirInSky, glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::mat4 proj = glm::perspectiveRH_ZO(glm::radians(60.0f), 1.0f, 0.1f, 1000.0f);
    const glm::mat4 viewProj = proj * view;
    const glm::mat4 invViewProj = glm::inverse(viewProj);

    GpuTexture target = device.createRenderTarget(kSize, kSize);
    CommandBuffer cmd = device.acquireCommandBuffer();
    if (renderer.begin(cmd, target.get(), kSize, kSize, glm::vec4(sky.horizonColor, 1.0f))) {
        renderer.setCamera(viewProj, cameraPos);
        skyRenderer.draw(renderer, invViewProj, cameraPos, sky);
        renderer.end();
    }
    REQUIRE(cmd.submitAndWait());

    const mge_test::Image image{kSize, kSize, device.downloadTexture(target, kSize, kSize)};
    checkGolden("sky_sunset_sun", image);
}
