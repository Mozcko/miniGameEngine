#include "GpuTestContext.h"

#include <mge/core/Application.h>
#include <mge/gpu/CommandBuffer.h>
#include <mge/gpu/Swapchain.h>

#include <set>

using namespace mge;
using mge_test::GpuTestContext;

TEST_CASE("Vsync por defecto en la config") {
    CHECK(ApplicationConfig{}.vsync);
}

TEST_CASE("choosePresentMode: vsync, o el mejor modo sin vsync disponible") {
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

TEST_CASE("chooseDepthFormat: el más preciso disponible, D16 como último recurso") {
    auto only = [](std::set<SDL_GPUTextureFormat> formats) {
        return [formats](SDL_GPUTextureFormat f) { return formats.count(f) != 0; };
    };
    CHECK(chooseDepthFormat(only({SDL_GPU_TEXTUREFORMAT_D16_UNORM, SDL_GPU_TEXTUREFORMAT_D24_UNORM,
                                  SDL_GPU_TEXTUREFORMAT_D32_FLOAT})) == SDL_GPU_TEXTUREFORMAT_D32_FLOAT);
    CHECK(chooseDepthFormat(only({SDL_GPU_TEXTUREFORMAT_D16_UNORM, SDL_GPU_TEXTUREFORMAT_D24_UNORM})) ==
          SDL_GPU_TEXTUREFORMAT_D24_UNORM);
    CHECK(chooseDepthFormat(only({})) == SDL_GPU_TEXTUREFORMAT_D16_UNORM);
}

TEST_CASE("GPU: el device elige un depth de 24 o 32 bits y se puede usar en un render pass") {
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
