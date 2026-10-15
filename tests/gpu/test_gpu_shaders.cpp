#include "GpuTestContext.h"

#include <mge/gpu/CommandBuffer.h>
#include <mge/gpu/Pipeline.h>

#include <shaders/test_triangle_frag.h>
#include <shaders/test_triangle_vert.h>

#include <cstdint>

using namespace mge;
using mge_test::GpuTestContext;

TEST_CASE("Shaders embebidos: SPIR-V válido con la etapa correcta") {
    // Número mágico de SPIR-V (little endian): 0x07230203
    const ShaderCode& vertex = shaders::kTestTriangleVert;
    REQUIRE(vertex.size >= 20);
    CHECK(vertex.size % 4 == 0);
    CHECK(vertex.code[0] == 0x03);
    CHECK(vertex.code[1] == 0x02);
    CHECK(vertex.code[2] == 0x23);
    CHECK(vertex.code[3] == 0x07);
    CHECK(vertex.stage == ShaderStage::Vertex);
    CHECK(shaders::kTestTriangleFrag.stage == ShaderStage::Fragment);
    CHECK(reinterpret_cast<std::uintptr_t>(vertex.code) % 4 == 0); // Alineado a palabras de 32 bits
}

TEST_CASE("GPU: triángulo con un uniform del fragmento, dibujado y leído de vuelta") {
    GpuTestContext gpu;
    if (!gpu.available()) return;
    GpuDevice& device = gpu.device();

    ShaderResources fragmentResources;
    fragmentResources.uniformBuffers = 1;
    const GpuShader vertex = device.createShader(shaders::kTestTriangleVert);
    const GpuShader fragment = device.createShader(shaders::kTestTriangleFrag, fragmentResources);
    REQUIRE(vertex);
    REQUIRE(fragment);

    GraphicsPipelineDesc desc;
    desc.vertexShader = &vertex;
    desc.fragmentShader = &fragment;
    desc.gpuVertexInput = false;
    desc.cullMode = SDL_GPU_CULLMODE_NONE;
    const GpuGraphicsPipeline pipeline = device.createGraphicsPipeline(desc);
    REQUIRE(pipeline);

    constexpr std::uint32_t kSize = 32;
    GpuTexture target = device.createRenderTarget(kSize, kSize);
    CommandBuffer cmd = device.acquireCommandBuffer();
    const float color[4] = {0.0f, 1.0f, 0.0f, 1.0f};
    SDL_PushGPUFragmentUniformData(cmd.get(), 0, color, sizeof(color));
    {
        ColorTarget clear;
        clear.texture = target.get();
        clear.clearColor = {0.0f, 0.0f, 1.0f, 1.0f};
        RenderPass pass = cmd.beginRenderPass(clear);
        SDL_BindGPUGraphicsPipeline(pass.get(), pipeline.get());
        SDL_DrawGPUPrimitives(pass.get(), 3, 1, 0, 0);
    }
    REQUIRE(cmd.submitAndWait());

    const auto pixels = device.downloadTexture(target, kSize, kSize);
    REQUIRE(pixels.size() == kSize * kSize * 4);
    auto pixel = [&](std::uint32_t x, std::uint32_t y) { return &pixels[(y * kSize + x) * 4]; };
    // Centro: el triángulo (verde, del uniform). Esquinas: el fondo (azul)
    CHECK(pixel(16, 16)[0] == 0);
    CHECK(pixel(16, 16)[1] == 255);
    CHECK(pixel(16, 16)[2] == 0);
    for (auto [x, y] : {std::pair{0u, 0u}, std::pair{31u, 0u}, std::pair{0u, 31u}, std::pair{31u, 31u}}) {
        CHECK(pixel(x, y)[1] == 0);
        CHECK(pixel(x, y)[2] == 255);
    }
    CHECK(GpuTestContext::validationMessages().empty());
}
