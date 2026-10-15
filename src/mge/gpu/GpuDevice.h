#pragma once

#include <mge/gpu/CommandBuffer.h>
#include <mge/gpu/GpuMesh.h>
#include <mge/gpu/Handles.h>
#include <mge/procgen/MeshData.h>

#include <cstdint>
#include <memory>
#include <vector>

namespace mge {

    struct GpuDeviceOptions {
#ifdef MGE_GPU_DEBUG_DEFAULT
        bool debug = true; // Capas de validación (si están instaladas): errores de uso de la API en el log
#else
        bool debug = false;
#endif
        const char* driver = nullptr; // nullptr = el mejor disponible ("vulkan", "metal", "direct3d12")
    };

    // Dueño del SDL_GPUDevice y fábrica de recursos. Todos los recursos que crea deben destruirse antes
    // que el device. Necesita el subsistema de video de SDL inicializado (para cargar Vulkan).
    class GpuDevice {
    public:
        // nullptr si no hay GPU compatible (el motivo queda en el log)
        static std::unique_ptr<GpuDevice> create(const GpuDeviceOptions& options = {});
        ~GpuDevice();
        GpuDevice(const GpuDevice&) = delete;
        GpuDevice& operator=(const GpuDevice&) = delete;

        SDL_GPUDevice* get() const { return m_device; }
        const char* driverName() const;

        // --- Recursos ---
        GpuBuffer createBuffer(SDL_GPUBufferUsageFlags usage, std::uint32_t size);
        GpuTransferBuffer createTransferBuffer(SDL_GPUTransferBufferUsage usage, std::uint32_t size);
        GpuTexture createTexture(const SDL_GPUTextureCreateInfo& info);
        // Textura 2D en la que se puede dibujar y luego leer (o muestrear)
        GpuTexture createRenderTarget(std::uint32_t width, std::uint32_t height,
                                      SDL_GPUTextureFormat format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM);
        GpuSampler createSampler(const SDL_GPUSamplerCreateInfo& info);
        GpuShader createShader(const SDL_GPUShaderCreateInfo& info);
        GpuGraphicsPipeline createGraphicsPipeline(const SDL_GPUGraphicsPipelineCreateInfo& info);
        GpuComputePipeline createComputePipeline(const SDL_GPUComputePipelineCreateInfo& info);

        // --- Trabajo ---
        CommandBuffer acquireCommandBuffer();
        // Copia bytes de la CPU a un buffer de la GPU (con su propio command buffer, sin esperar)
        bool uploadToBuffer(const GpuBuffer& buffer, const void* data, std::uint32_t size);
        // Sube una MeshData (vértices entrelazados + índices). Malla vacía → GpuMesh vacía
        GpuMesh uploadMesh(const MeshData& mesh);
        // Lee una textura RGBA8 a la CPU (bloquea hasta que la GPU termine). Para tests y capturas.
        std::vector<std::uint8_t> downloadTexture(const GpuTexture& texture, std::uint32_t width,
                                                  std::uint32_t height);
        void waitIdle();

    private:
        explicit GpuDevice(SDL_GPUDevice* device) : m_device(device) {}

        SDL_GPUDevice* m_device = nullptr;
    };
}
