#include "GpuTestContext.h"

#include <mge/gpu/CommandBuffer.h>
#include <mge/gpu/Pipeline.h>

#include <shaders/test_sdf_raymarch_comp.h>
#include <shaders/test_triangle_frag.h>
#include <shaders/test_triangle_vert.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

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
    CHECK(shaders::kTestSdfRaymarchComp.stage == ShaderStage::Compute);
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
}

TEST_CASE("GPU compute: SDF y raymarching ejecutados en GPU con acuerdo matemático") {
    const ShaderCode& computeShader = shaders::kTestSdfRaymarchComp;
    REQUIRE(computeShader.size >= 20);
    CHECK(computeShader.stage == ShaderStage::Compute);

    GpuTestContext gpu;
    if (!gpu.available()) return;
    GpuDevice& device = gpu.device();

    ComputePipelineDesc desc;
    desc.bytecode = computeShader;
    desc.threadcountX = 1;
    desc.threadcountY = 1;
    desc.threadcountZ = 1;
    desc.readwriteStorageBuffers = 1;

    GpuComputePipeline pipeline = device.createComputePipeline(desc);
    REQUIRE(pipeline);

    constexpr std::uint32_t kFloatCount = 20;
    constexpr std::uint32_t kByteSize = kFloatCount * sizeof(float);

    GpuBuffer buffer = device.createBuffer(SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE, kByteSize);
    REQUIRE(buffer);

    CommandBuffer cmd = device.acquireCommandBuffer();
    REQUIRE(cmd);
    {
        ComputePass pass = cmd.beginComputePass(buffer);
        pass.bindPipeline(pipeline);
        pass.dispatch(1, 1, 1);
    }
    REQUIRE(cmd.submitAndWait());

    const std::vector<std::uint8_t> bytes = device.downloadBuffer(buffer, kByteSize);
    REQUIRE(bytes.size() == kByteSize);

    std::vector<float> results(kFloatCount);
    std::memcpy(results.data(), bytes.data(), kByteSize);

    // Primitivas SDF
    CHECK(results[0] == doctest::Approx(1.0f));  // sdSphere({2, 0, 0}, 1.0)
    CHECK(results[1] == doctest::Approx(-1.0f)); // sdSphere({0, 0, 0}, 1.0)
    CHECK(results[2] == doctest::Approx(2.0f));  // sdBox({3, 0, 0}, {1, 1, 1})
    CHECK(results[3] == doctest::Approx(-0.5f)); // sdTorus({2, 0, 0}, 2.0, 0.5)
    CHECK(results[4] == doctest::Approx(1.5f));  // sdCapsule({0, 3, 0}, ...)
    CHECK(results[5] == doctest::Approx(1.0f));  // sdCylinder({0, 3, 0}, 1.0, 2.0)
    CHECK(results[6] == doctest::Approx(3.0f));  // sdPlane({0, 4, 0}, ...)

    // Booleanas
    CHECK(results[7] == doctest::Approx(1.0f));  // opUnion
    CHECK(results[8] == doctest::Approx(2.0f));  // opSubtract
    CHECK(results[9] == doctest::Approx(2.0f));  // opIntersect
    CHECK(results[10] == doctest::Approx(1.0f)); // opRound
    CHECK(results[11] == doctest::Approx(1.0f)); // opSmoothUnion con k=0

    // Raymarching: rayo desde (0, 1, -5) hacia esfera en (0, 1, 0) radio 1 -> t debe ser ~4.0
    CHECK(results[12] == doctest::Approx(4.0f).epsilon(0.01f));
    CHECK(results[13] > 0.0f); // steps

    // Normal en la cara frontal de la esfera -> (0, 0, -1)
    CHECK(results[14] == doctest::Approx(0.0f).epsilon(0.02f));
    CHECK(results[15] == doctest::Approx(0.0f).epsilon(0.02f));
    CHECK(results[16] == doctest::Approx(-1.0f).epsilon(0.02f));

    // AO en plano despejado -> 1.0
    CHECK(results[17] == doctest::Approx(1.0f).epsilon(0.01f));

    // Sombra suave despejada -> 1.0; sombra oculta -> 0.0
    CHECK(results[18] == doctest::Approx(1.0f).epsilon(0.01f));
    CHECK(results[19] == doctest::Approx(0.0f));
}
