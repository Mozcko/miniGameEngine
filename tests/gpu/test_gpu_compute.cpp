#include "GpuTestContext.h"

#include <mge/gpu/CommandBuffer.h>
#include <mge/gpu/Pipeline.h>

#include <shaders/test_multiply_comp.h>
#include <shaders/test_square_comp.h>

#include <cstdint>
#include <cstring>
#include <vector>

using namespace mge;
using mge_test::GpuTestContext;

TEST_CASE("GPU compute: shader que escribe i*i en un buffer de 1024 uints") {
    GpuTestContext gpu;
    if (!gpu.available()) return;
    GpuDevice& device = gpu.device();

    ComputePipelineDesc desc;
    desc.bytecode = shaders::kTestSquareComp;
    desc.threadcountX = 64;
    desc.threadcountY = 1;
    desc.threadcountZ = 1;
    desc.readwriteStorageBuffers = 1;

    GpuComputePipeline pipeline = device.createComputePipeline(desc);
    REQUIRE(pipeline);

    constexpr std::uint32_t kCount = 1024;
    constexpr std::uint32_t kByteSize = kCount * sizeof(std::uint32_t);

    GpuBuffer buffer = device.createBuffer(SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE, kByteSize);
    REQUIRE(buffer);

    CommandBuffer cmd = device.acquireCommandBuffer();
    REQUIRE(cmd);
    {
        ComputePass pass = cmd.beginComputePass(buffer);
        pass.bindPipeline(pipeline);
        pass.dispatch(kCount / 64, 1, 1);
    }
    REQUIRE(cmd.submitAndWait());

    const std::vector<std::uint8_t> bytes = device.downloadBuffer(buffer, kByteSize);
    REQUIRE(bytes.size() == kByteSize);

    std::vector<std::uint32_t> results(kCount);
    std::memcpy(results.data(), bytes.data(), kByteSize);
    for (std::uint32_t i = 0; i < kCount; ++i) {
        CHECK(results[i] == i * i);
    }
}

TEST_CASE("GPU compute: dispatch usando métodos directos de CommandBuffer") {
    GpuTestContext gpu;
    if (!gpu.available()) return;
    GpuDevice& device = gpu.device();

    ComputePipelineDesc desc;
    desc.bytecode = shaders::kTestSquareComp;
    desc.threadcountX = 64;
    desc.threadcountY = 1;
    desc.threadcountZ = 1;
    desc.readwriteStorageBuffers = 1;

    GpuComputePipeline pipeline = device.createComputePipeline(desc);
    REQUIRE(pipeline);

    constexpr std::uint32_t kCount = 1024;
    constexpr std::uint32_t kByteSize = kCount * sizeof(std::uint32_t);

    GpuBuffer buffer = device.createBuffer(SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE, kByteSize);
    REQUIRE(buffer);

    CommandBuffer cmd = device.acquireCommandBuffer();
    REQUIRE(cmd);
    {
        ComputePass pass = cmd.beginComputePass(buffer);
        cmd.bindComputePipeline(pipeline);
        cmd.dispatch(kCount / 64, 1, 1);
        cmd.endComputePass();
    }
    REQUIRE(cmd.submitAndWait());

    const std::vector<std::uint8_t> bytes = device.downloadBuffer(buffer, kByteSize);
    REQUIRE(bytes.size() == kByteSize);

    std::vector<std::uint32_t> results(kCount);
    std::memcpy(results.data(), bytes.data(), kByteSize);
    for (std::uint32_t i = 0; i < kCount; ++i) {
        CHECK(results[i] == i * i);
    }
}

TEST_CASE("GPU compute: pushComputeUniform con multiplicador") {
    GpuTestContext gpu;
    if (!gpu.available()) return;
    GpuDevice& device = gpu.device();

    ComputePipelineDesc desc;
    desc.bytecode = shaders::kTestMultiplyComp;
    desc.threadcountX = 64;
    desc.threadcountY = 1;
    desc.threadcountZ = 1;
    desc.readwriteStorageBuffers = 1;
    desc.uniformBuffers = 1;

    GpuComputePipeline pipeline = device.createComputePipeline(desc);
    REQUIRE(pipeline);

    constexpr std::uint32_t kCount = 1024;
    constexpr std::uint32_t kByteSize = kCount * sizeof(std::uint32_t);

    GpuBuffer buffer = device.createBuffer(SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE, kByteSize);
    REQUIRE(buffer);

    CommandBuffer cmd = device.acquireCommandBuffer();
    REQUIRE(cmd);
    {
        ComputePass pass = cmd.beginComputePass(buffer);
        pass.bindPipeline(pipeline);
        constexpr std::uint32_t kMultiplier = 7;
        cmd.pushComputeUniform(0, kMultiplier);
        pass.dispatch(kCount / 64, 1, 1);
    }
    REQUIRE(cmd.submitAndWait());

    const std::vector<std::uint8_t> bytes = device.downloadBuffer(buffer, kByteSize);
    REQUIRE(bytes.size() == kByteSize);

    std::vector<std::uint32_t> results(kCount);
    std::memcpy(results.data(), bytes.data(), kByteSize);
    for (std::uint32_t i = 0; i < kCount; ++i) {
        CHECK(results[i] == i * 7);
    }
}
