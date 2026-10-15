#pragma once

#include <mge/gpu/Handles.h>

#include <SDL3/SDL_gpu.h>

namespace mge {

    enum class BlendMode {
        None,     // Opaco
        Alpha,    // src * a + dst * (1 - a)
        Additive, // src * a + dst
    };

    // Descripción simplificada de un pipeline gráfico (lo común en el motor). Para casos raros se puede
    // usar SDL_GPUGraphicsPipelineCreateInfo directo con GpuDevice::createGraphicsPipeline.
    struct GraphicsPipelineDesc {
        const GpuShader* vertexShader = nullptr;
        const GpuShader* fragmentShader = nullptr;
        SDL_GPUTextureFormat colorFormat = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        // INVALID = sin buffer de profundidad
        SDL_GPUTextureFormat depthFormat = SDL_GPU_TEXTUREFORMAT_INVALID;
        bool depthTest = true; // Solo si hay depthFormat
        bool depthWrite = true;
        // true: vértices GpuVertex en el slot 0 (mallas). false: sin buffers (el shader usa gl_VertexIndex)
        bool gpuVertexInput = true;
        SDL_GPUPrimitiveType primitive = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        SDL_GPUCullMode cullMode = SDL_GPU_CULLMODE_BACK;
        SDL_GPUFrontFace frontFace = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        BlendMode blend = BlendMode::None;
    };
}
