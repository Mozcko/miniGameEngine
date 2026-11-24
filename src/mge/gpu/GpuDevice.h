#pragma once

#include <mge/gpu/CommandBuffer.h>
#include <mge/gpu/GpuMesh.h>
#include <mge/gpu/GpuResourceTracker.h>
#include <mge/gpu/Handles.h>
#include <mge/gpu/Pipeline.h>
#include <mge/gpu/Shader.h>
#include <mge/procgen/MeshData.h>

#include <cstdint>
#include <memory>
#include <vector>

namespace mge {

    struct GpuDeviceOptions {
#ifdef MGE_GPU_DEBUG_DEFAULT
        bool debug = true; // Validation layers (if installed): API usage errors logged
#else
        bool debug = false;
#endif
        const char* driver = nullptr; // nullptr = best available ("vulkan", "metal", "direct3d12")
    };

    // Owner of SDL_GPUDevice and resource factory. All resources it creates must be destroyed before
    // the device. Requires the SDL video subsystem to be initialized (to load Vulkan).
    class GpuDevice {
    public:
        // nullptr if no compatible GPU is available (reason is logged)
        static std::unique_ptr<GpuDevice> create(const GpuDeviceOptions& options = {});
        ~GpuDevice();
        GpuDevice(const GpuDevice&) = delete;
        GpuDevice& operator=(const GpuDevice&) = delete;

        SDL_GPUDevice* get() const { return m_device; }
        const char* driverName() const; // "vulkan", "metal", "direct3d12"
        // Depth format chosen when creating the device (chooseDepthFormat: D32/D24/D16)
        SDL_GPUTextureFormat depthFormat() const { return m_depthFormat; }

        // --- Resources ---
        GpuBuffer createBuffer(SDL_GPUBufferUsageFlags usage, std::uint32_t size);
        GpuTransferBuffer createTransferBuffer(SDL_GPUTransferBufferUsage usage, std::uint32_t size);
        GpuTexture createTexture(const SDL_GPUTextureCreateInfo& info);
        // 2D texture that can be rendered to and then read back (or sampled)
        GpuTexture createRenderTarget(std::uint32_t width, std::uint32_t height,
                                      SDL_GPUTextureFormat format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM);
        // Depth buffer (depthFormat()) for a render pass of the given size
        GpuTexture createDepthTarget(std::uint32_t width, std::uint32_t height);
        GpuSampler createSampler(const SDL_GPUSamplerCreateInfo& info);
        GpuShader createShader(const SDL_GPUShaderCreateInfo& info);
        // Embedded vertex or fragment shader (mge_add_shaders), with the resources it uses
        GpuShader createShader(const ShaderCode& code, const ShaderResources& resources = {});
        GpuGraphicsPipeline createGraphicsPipeline(const SDL_GPUGraphicsPipelineCreateInfo& info);
        GpuGraphicsPipeline createGraphicsPipeline(const GraphicsPipelineDesc& desc);
        GpuComputePipeline createComputePipeline(const SDL_GPUComputePipelineCreateInfo& info);
        GpuComputePipeline createComputePipeline(const ComputePipelineDesc& desc);

        // --- Work ---
        CommandBuffer acquireCommandBuffer();
        // Copies bytes from the CPU to a GPU buffer (using its own command buffer, without waiting)
        // cycle = true for buffers rewritten every frame (SDL avoids overwriting data the GPU is still
        // using)
        bool uploadToBuffer(const GpuBuffer& buffer, const void* data, std::uint32_t size,
                            bool cycle = false);
        // Updates a [offset, offset + size) range of a GPU buffer via a copy pass.
        // cycle = true only applies when overwriting from offset == 0.
        bool updateBuffer(const GpuBuffer& buffer, const void* data, std::uint32_t size,
                          std::uint32_t offset = 0, bool cycle = false);
        // Copies RGBA8 pixels from the CPU to a texture region (rows in texture order)
        bool uploadToTexture(const GpuTexture& texture, std::uint32_t x, std::uint32_t y, std::uint32_t width,
                             std::uint32_t height, const void* rgba);
        // Uploads a MeshData (interleaved vertices + indices). Empty mesh → empty GpuMesh
        GpuMesh uploadMesh(const MeshData& mesh);
        // Reads an RGBA8 texture back to the CPU (blocks until the GPU finishes). For tests and screenshots.
        std::vector<std::uint8_t> downloadTexture(const GpuTexture& texture, std::uint32_t width,
                                                  std::uint32_t height);
        // Reads bytes from a GPU buffer back to the CPU (blocks until the GPU finishes). For tests and
        // tools.
        std::vector<std::uint8_t> downloadBuffer(const GpuBuffer& buffer, std::size_t size);
        void waitIdle();

        // GPU resource tracker (for bindings and preventing hangs on shutdown)
        GpuResourceTracker& tracker() { return m_tracker; }
        const GpuResourceTracker& tracker() const { return m_tracker; }

    private:
        explicit GpuDevice(SDL_GPUDevice* device) : m_device(device) {}

        SDL_GPUDevice* m_device = nullptr;
        SDL_GPUTextureFormat m_depthFormat = SDL_GPU_TEXTUREFORMAT_D16_UNORM;
        GpuResourceTracker m_tracker;
    };
}
