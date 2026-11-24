#include "GpuTestContext.h"

#include <mge/core/Application.h>
#include <mge/gpu/CommandBuffer.h>
#include <mge/gpu/Swapchain.h>

#include <set>

using namespace mge;
using mge_test::GpuTestContext;

TEST_CASE("Vsync enabled by default in config") {
    CHECK(ApplicationConfig{}.vsync);
}

TEST_CASE("choosePresentMode: vsync, or best available non-vsync mode") {
    auto only = [](std::set<SDL_GPUPresentMode> modes) {
        return [modes](SDL_GPUPresentMode m) { return modes.count(m) != 0; };
    };
    const auto all =
        only({SDL_GPU_PRESENTMODE_VSYNC, SDL_GPU_PRESENTMODE_MAILBOX, SDL_GPU_PRESENTMODE_IMMEDIATE});
    CHECK(choosePresentMode(true, all) == SDL_GPU_PRESENTMODE_VSYNC);
    CHECK(choosePresentMode(false, all) == SDL_GPU_PRESENTMODE_MAILBOX);
    CHECK(choosePresentMode(false, only({SDL_GPU_PRESENTMODE_VSYNC, SDL_GPU_PRESENTMODE_IMMEDIATE})) ==
          SDL_GPU_PRESENTMODE_IMMEDIATE);
    CHECK(choosePresentMode(false, only({SDL_GPU_PRESENTMODE_VSYNC})) == SDL_GPU_PRESENTMODE_VSYNC);
    CHECK(std::string(presentModeName(SDL_GPU_PRESENTMODE_MAILBOX)) == "mailbox");
}

TEST_CASE("chooseDepthFormat: most precise available, D16 as fallback") {
    auto only = [](std::set<SDL_GPUTextureFormat> formats) {
        return [formats](SDL_GPUTextureFormat f) { return formats.count(f) != 0; };
    };
    CHECK(chooseDepthFormat(only({SDL_GPU_TEXTUREFORMAT_D16_UNORM, SDL_GPU_TEXTUREFORMAT_D24_UNORM,
                                  SDL_GPU_TEXTUREFORMAT_D32_FLOAT})) == SDL_GPU_TEXTUREFORMAT_D32_FLOAT);
    CHECK(chooseDepthFormat(only({SDL_GPU_TEXTUREFORMAT_D16_UNORM, SDL_GPU_TEXTUREFORMAT_D24_UNORM})) ==
          SDL_GPU_TEXTUREFORMAT_D24_UNORM);
    CHECK(chooseDepthFormat(only({})) == SDL_GPU_TEXTUREFORMAT_D16_UNORM);
}

TEST_CASE("GPU: device selects a 24- or 32-bit depth format usable in a render pass") {
    GpuTestContext gpu;
    if (!gpu.available()) return;
    const SDL_GPUTextureFormat depth = gpu.device().depthFormat();
    CHECK((depth == SDL_GPU_TEXTUREFORMAT_D32_FLOAT || depth == SDL_GPU_TEXTUREFORMAT_D24_UNORM));

    GpuTexture color = gpu.device().createRenderTarget(8, 8);
    GpuTexture depthTarget = gpu.device().createDepthTarget(8, 8);
    REQUIRE(depthTarget);
    CommandBuffer cmd = gpu.device().acquireCommandBuffer();
    {
        ColorTarget target;
        target.texture = color.get();
        DepthTarget depthInfo;
        depthInfo.texture = depthTarget.get();
        RenderPass pass = cmd.beginRenderPass(target, &depthInfo);
        CHECK(pass);
    }
    CHECK(cmd.submitAndWait());
}
