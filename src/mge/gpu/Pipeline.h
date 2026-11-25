#pragma once

#include <mge/gpu/Handles.h>
#include <mge/gpu/Shader.h>

#include <SDL3/SDL_gpu.h>

#include <cstdint>

namespace mge {

    enum class BlendMode {
        None,     // Opaque
        Alpha,    // src * a + dst * (1 - a)
        Additive, // src * a + dst
    };

    // Simplified description of a graphics pipeline (common cases in the engine). For unusual cases,
    // SDL_GPUGraphicsPipelineCreateInfo can be used directly with GpuDevice::createGraphicsPipeline.
    struct GraphicsPipelineDesc {
        const GpuShader* vertexShader = nullptr;
        const GpuShader* fragmentShader = nullptr;
        SDL_GPUTextureFormat colorFormat = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        // INVALID = no depth buffer
        SDL_GPUTextureFormat depthFormat = SDL_GPU_TEXTUREFORMAT_INVALID;
        bool depthTest = true; // Only if depthFormat is set
        bool depthWrite = true;
        SDL_GPUCompareOp compareOp = SDL_GPU_COMPAREOP_LESS;
        // true: GpuVertex vertices in slot 0 (meshes). false: no vertex buffers (shader uses gl_VertexIndex)
        bool gpuVertexInput = true;
        SDL_GPUPrimitiveType primitive = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        SDL_GPUCullMode cullMode = SDL_GPU_CULLMODE_BACK;
        SDL_GPUFrontFace frontFace = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        BlendMode blend = BlendMode::None;
    };

    // Simplified description of a compute pipeline.
    struct ComputePipelineDesc {
        ShaderCode bytecode;
        const char* entrypoint = "main";
        std::uint32_t threadcountX = 1;
        std::uint32_t threadcountY = 1;
        std::uint32_t threadcountZ = 1;
        std::uint32_t samplers = 0;
        std::uint32_t readonlyStorageTextures = 0;
        std::uint32_t readonlyStorageBuffers = 0;
        std::uint32_t readwriteStorageTextures = 0;
        std::uint32_t readwriteStorageBuffers = 0;
        std::uint32_t uniformBuffers = 0;
    };
}
