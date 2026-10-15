#include <mge/gpu/GpuDevice.h>

#include <mge/core/Log.h>

#include <cstring>

namespace mge {

    namespace {
        template <typename Handle, typename T> Handle wrap(SDL_GPUDevice* device, T* raw, const char* what) {
            if (!raw) MGE_LOG_ERROR("Gpu", "No se pudo crear ", what, ": ", SDL_GetError());
            return Handle(device, raw);
        }
    }

    std::unique_ptr<GpuDevice> GpuDevice::create(const GpuDeviceOptions& options) {
        // Por ahora solo SPIR-V (Vulkan); MSL/DXIL llegan con macOS/Windows (#29)
        SDL_GPUDevice* device =
            SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, options.debug, options.driver);
        if (!device) {
            MGE_LOG_ERROR("Gpu", "No hay una GPU compatible (SDL GPU, SPIR-V): ", SDL_GetError());
            return nullptr;
        }
        MGE_LOG_INFO("Gpu", "Device creado: ", SDL_GetGPUDeviceDriver(device),
                     options.debug ? " (modo debug: validación si las capas están instaladas)" : "");
        return std::unique_ptr<GpuDevice>(new GpuDevice(device));
    }

    GpuDevice::~GpuDevice() {
        if (m_device) {
            SDL_WaitForGPUIdle(m_device);
            SDL_DestroyGPUDevice(m_device);
        }
    }

    const char* GpuDevice::driverName() const {
        return SDL_GetGPUDeviceDriver(m_device);
    }

    GpuBuffer GpuDevice::createBuffer(SDL_GPUBufferUsageFlags usage, std::uint32_t size) {
        SDL_GPUBufferCreateInfo info{};
        info.usage = usage;
        info.size = size;
        return wrap<GpuBuffer>(m_device, SDL_CreateGPUBuffer(m_device, &info), "un buffer");
    }

    GpuTransferBuffer GpuDevice::createTransferBuffer(SDL_GPUTransferBufferUsage usage, std::uint32_t size) {
        SDL_GPUTransferBufferCreateInfo info{};
        info.usage = usage;
        info.size = size;
        return wrap<GpuTransferBuffer>(m_device, SDL_CreateGPUTransferBuffer(m_device, &info),
                                       "un transfer buffer");
    }

    GpuTexture GpuDevice::createTexture(const SDL_GPUTextureCreateInfo& info) {
        return wrap<GpuTexture>(m_device, SDL_CreateGPUTexture(m_device, &info), "una textura");
    }

    GpuTexture GpuDevice::createRenderTarget(std::uint32_t width, std::uint32_t height,
                                             SDL_GPUTextureFormat format) {
        SDL_GPUTextureCreateInfo info{};
        info.type = SDL_GPU_TEXTURETYPE_2D;
        info.format = format;
        info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
        info.width = width;
        info.height = height;
        info.layer_count_or_depth = 1;
        info.num_levels = 1;
        info.sample_count = SDL_GPU_SAMPLECOUNT_1;
        return createTexture(info);
    }

    GpuSampler GpuDevice::createSampler(const SDL_GPUSamplerCreateInfo& info) {
        return wrap<GpuSampler>(m_device, SDL_CreateGPUSampler(m_device, &info), "un sampler");
    }

    GpuShader GpuDevice::createShader(const SDL_GPUShaderCreateInfo& info) {
        return wrap<GpuShader>(m_device, SDL_CreateGPUShader(m_device, &info), "un shader");
    }

    GpuGraphicsPipeline GpuDevice::createGraphicsPipeline(const SDL_GPUGraphicsPipelineCreateInfo& info) {
        return wrap<GpuGraphicsPipeline>(m_device, SDL_CreateGPUGraphicsPipeline(m_device, &info),
                                         "un pipeline gráfico");
    }

    GpuComputePipeline GpuDevice::createComputePipeline(const SDL_GPUComputePipelineCreateInfo& info) {
        return wrap<GpuComputePipeline>(m_device, SDL_CreateGPUComputePipeline(m_device, &info),
                                        "un pipeline de cómputo");
    }

    CommandBuffer GpuDevice::acquireCommandBuffer() {
        SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(m_device);
        if (!cmd) MGE_LOG_ERROR("Gpu", "No se pudo obtener un command buffer: ", SDL_GetError());
        return CommandBuffer(m_device, cmd);
    }

    bool GpuDevice::uploadToBuffer(const GpuBuffer& buffer, const void* data, std::uint32_t size) {
        if (!buffer || size == 0) return false;
        GpuTransferBuffer staging = createTransferBuffer(SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, size);
        if (!staging) return false;
        void* mapped = SDL_MapGPUTransferBuffer(m_device, staging.get(), false);
        if (!mapped) {
            MGE_LOG_ERROR("Gpu", "No se pudo mapear el transfer buffer: ", SDL_GetError());
            return false;
        }
        std::memcpy(mapped, data, size);
        SDL_UnmapGPUTransferBuffer(m_device, staging.get());

        CommandBuffer cmd = acquireCommandBuffer();
        if (!cmd) return false;
        {
            CopyPass copy = cmd.beginCopyPass();
            SDL_GPUTransferBufferLocation source{staging.get(), 0};
            SDL_GPUBufferRegion destination{buffer.get(), 0, size};
            SDL_UploadToGPUBuffer(copy.get(), &source, &destination, false);
        }
        // El transfer buffer se puede liberar ya: SDL lo mantiene vivo hasta que la copia termine
        return cmd.submit();
    }

    GpuMesh GpuDevice::uploadMesh(const MeshData& mesh) {
        GpuMesh result;
        if (mesh.indices.empty()) return result;
        const std::vector<GpuVertex> vertices = interleave(mesh);
        const auto vertexBytes = static_cast<std::uint32_t>(vertices.size() * sizeof(GpuVertex));
        const auto indexBytes = static_cast<std::uint32_t>(mesh.indices.size() * sizeof(std::uint32_t));
        result.vertices = createBuffer(SDL_GPU_BUFFERUSAGE_VERTEX, vertexBytes);
        result.indices = createBuffer(SDL_GPU_BUFFERUSAGE_INDEX, indexBytes);
        if (!uploadToBuffer(result.vertices, vertices.data(), vertexBytes) ||
            !uploadToBuffer(result.indices, mesh.indices.data(), indexBytes))
            return GpuMesh{};
        result.vertexCount = static_cast<std::uint32_t>(vertices.size());
        result.indexCount = static_cast<std::uint32_t>(mesh.indices.size());
        return result;
    }

    std::vector<std::uint8_t> GpuDevice::downloadTexture(const GpuTexture& texture, std::uint32_t width,
                                                         std::uint32_t height) {
        const std::uint32_t size = width * height * 4;
        GpuTransferBuffer readback = createTransferBuffer(SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD, size);
        CommandBuffer cmd = acquireCommandBuffer();
        if (!readback || !cmd) return {};
        {
            CopyPass copy = cmd.beginCopyPass();
            SDL_GPUTextureRegion source{};
            source.texture = texture.get();
            source.w = width;
            source.h = height;
            source.d = 1;
            SDL_GPUTextureTransferInfo destination{};
            destination.transfer_buffer = readback.get();
            SDL_DownloadFromGPUTexture(copy.get(), &source, &destination);
        }
        if (!cmd.submitAndWait()) return {};

        std::vector<std::uint8_t> pixels(size);
        const void* mapped = SDL_MapGPUTransferBuffer(m_device, readback.get(), false);
        if (!mapped) return {};
        std::memcpy(pixels.data(), mapped, size);
        SDL_UnmapGPUTransferBuffer(m_device, readback.get());
        return pixels;
    }

    void GpuDevice::waitIdle() {
        SDL_WaitForGPUIdle(m_device);
    }
}
