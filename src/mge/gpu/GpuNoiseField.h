#pragma once

#include <mge/gpu/CommandBuffer.h>
#include <mge/gpu/GpuDevice.h>
#include <mge/gpu/Handles.h>
#include <mge/procgen/Noise.h>

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

namespace mge {

    // Configuración para la generación de un campo de ruido en la GPU.
    struct GpuNoiseFieldDesc {
        std::uint32_t width = 32;
        std::uint32_t height = 32;
        std::uint32_t depth = 32;
        glm::vec3 origin{0.0f};
        glm::vec3 scale{1.0f};
        FractalSettings fractal{}; // octaves (default 5), frequency (1.0), lacunarity (2.0), gain (0.5)
    };

    // Generador de campo de densidad 3D (W×H×D floats) en la GPU usando compute shaders.
    // Implementa el mismo ruido simplex 3D y fBm que mge::Noise con la misma tabla de permutación.
    class GpuNoiseField {
    public:
        GpuNoiseField() = default;
        explicit GpuNoiseField(GpuDevice& device, const Noise& noise = Noise{});
        explicit GpuNoiseField(GpuDevice& device, std::uint32_t seed);

        // Cambia la semilla o el objeto Noise de referencia (resube la tabla de permutación a la GPU)
        void setNoise(const Noise& noise);
        void setSeed(std::uint32_t seed) { setNoise(Noise{seed}); }

        // Graba la generación en un CommandBuffer existente (asíncrono)
        void generate(CommandBuffer& cmd, const GpuNoiseFieldDesc& desc);

        // Genera sincrónicamente (adquiere un CommandBuffer, graba y ejecuta submitAndWait)
        void generate(const GpuNoiseFieldDesc& desc);
        void generate(std::uint32_t width, std::uint32_t height, std::uint32_t depth, const glm::vec3& origin,
                      const glm::vec3& scale, const FractalSettings& fractal = {});
        void generate(std::uint32_t width, std::uint32_t height, std::uint32_t depth, const glm::vec3& origin,
                      float uniformScale, const FractalSettings& fractal = {});

        // Descarga el campo de densidad generado a la CPU como vector de floats (bloqueante, para
        // tests/herramientas)
        std::vector<float> download() const;

        // Buffer de salida en la GPU (contiene W×H×D floats)
        const GpuBuffer& buffer() const { return m_densityBuffer; }
        SDL_GPUBuffer* get() const { return m_densityBuffer.get(); }
        explicit operator bool() const { return static_cast<bool>(m_densityBuffer); }

        const GpuNoiseFieldDesc& desc() const { return m_desc; }
        std::uint32_t width() const { return m_desc.width; }
        std::uint32_t height() const { return m_desc.height; }
        std::uint32_t depth() const { return m_desc.depth; }
        std::size_t totalElements() const {
            return static_cast<std::size_t>(m_desc.width) * m_desc.height * m_desc.depth;
        }

        // Índice lineal para la celda (x, y, z): x + y * width + z * (width * height)
        std::size_t index(std::uint32_t x, std::uint32_t y, std::uint32_t z) const {
            return static_cast<std::size_t>(x) + static_cast<std::size_t>(y) * m_desc.width +
                   static_cast<std::size_t>(z) * (m_desc.width * m_desc.height);
        }

    private:
        void initPipeline();

        GpuDevice* m_device = nullptr;
        GpuComputePipeline m_pipeline;
        GpuBuffer m_permBuffer;
        GpuBuffer m_densityBuffer;
        std::uint32_t m_allocatedBytes = 0;
        GpuNoiseFieldDesc m_desc{};
    };

} // namespace mge
