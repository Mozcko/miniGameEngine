#pragma once

#include <SDL3/SDL_gpu.h>

#include <utility>

namespace mge {

    // Recurso de la GPU con dueño único: se libera solo (con el device que lo creó) y no se puede copiar,
    // así que no hay dobles liberaciones. Se puede mover. Debe destruirse antes que su GpuDevice.
    template <typename T, void (*Release)(SDL_GPUDevice*, T*)> class GpuHandle {
    public:
        GpuHandle() = default;
        GpuHandle(SDL_GPUDevice* device, T* handle) : m_device(device), m_handle(handle) {}
        ~GpuHandle() { reset(); }

        GpuHandle(const GpuHandle&) = delete;
        GpuHandle& operator=(const GpuHandle&) = delete;
        GpuHandle(GpuHandle&& other) noexcept
            : m_device(std::exchange(other.m_device, nullptr)),
              m_handle(std::exchange(other.m_handle, nullptr)) {}
        GpuHandle& operator=(GpuHandle&& other) noexcept {
            if (this != &other) {
                reset();
                m_device = std::exchange(other.m_device, nullptr);
                m_handle = std::exchange(other.m_handle, nullptr);
            }
            return *this;
        }

        // Libera ya (SDL espera a que la GPU termine de usarlo antes de borrarlo de verdad)
        void reset() {
            if (m_handle) Release(m_device, m_handle);
            m_handle = nullptr;
            m_device = nullptr;
        }

        T* get() const { return m_handle; }
        explicit operator bool() const { return m_handle != nullptr; }

    private:
        SDL_GPUDevice* m_device = nullptr;
        T* m_handle = nullptr;
    };

    using GpuBuffer = GpuHandle<SDL_GPUBuffer, SDL_ReleaseGPUBuffer>;
    using GpuTransferBuffer = GpuHandle<SDL_GPUTransferBuffer, SDL_ReleaseGPUTransferBuffer>;
    using GpuTexture = GpuHandle<SDL_GPUTexture, SDL_ReleaseGPUTexture>;
    using GpuSampler = GpuHandle<SDL_GPUSampler, SDL_ReleaseGPUSampler>;
    using GpuShader = GpuHandle<SDL_GPUShader, SDL_ReleaseGPUShader>;
    using GpuGraphicsPipeline = GpuHandle<SDL_GPUGraphicsPipeline, SDL_ReleaseGPUGraphicsPipeline>;
    using GpuComputePipeline = GpuHandle<SDL_GPUComputePipeline, SDL_ReleaseGPUComputePipeline>;
}
