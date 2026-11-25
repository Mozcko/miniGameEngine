#pragma once

#include <mge/gpu/CommandBuffer.h>
#include <mge/gpu/GpuDevice.h>
#include <mge/gpu/Handles.h>
#include <mge/procgen/Noise.h>

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

namespace mge {

    // Configuration for generating a noise field on the GPU.
    struct GpuNoiseFieldDesc {
        std::uint32_t width = 32;
        std::uint32_t height = 32;
        std::uint32_t depth = 32;
        glm::vec3 origin{0.0f};
        glm::vec3 scale{1.0f};
        FractalSettings fractal{}; // octaves (default 5), frequency (1.0), lacunarity (2.0), gain (0.5)
    };

    // 3D density field generator (W×H×D floats) on the GPU using compute shaders.
    // Implements the same 3D simplex noise and fBm as mge::Noise with the same permutation table.
    class GpuNoiseField {
    public:
        GpuNoiseField() = default;
        explicit GpuNoiseField(GpuDevice& device, const Noise& noise = Noise{});
        explicit GpuNoiseField(GpuDevice& device, std::uint32_t seed);

        // Changes the seed or reference Noise object (re-uploads the permutation table to the GPU)
        void setNoise(const Noise& noise);
        void setSeed(std::uint32_t seed) { setNoise(Noise{seed}); }

        // Records generation into an existing CommandBuffer (asynchronous)
        void generate(CommandBuffer& cmd, const GpuNoiseFieldDesc& desc);

        // Generates synchronously (acquires a CommandBuffer, records, and runs submitAndWait)
        void generate(const GpuNoiseFieldDesc& desc);
        void generate(std::uint32_t width, std::uint32_t height, std::uint32_t depth, const glm::vec3& origin,
                      const glm::vec3& scale, const FractalSettings& fractal = {});
        void generate(std::uint32_t width, std::uint32_t height, std::uint32_t depth, const glm::vec3& origin,
                      float uniformScale, const FractalSettings& fractal = {});

        // Downloads the generated density field to the CPU as a float vector (blocking, for
        // tests/tools)
        std::vector<float> download() const;

        // Output buffer on the GPU (contains W×H×D floats)
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

        // Linear index for cell (x, y, z): x + y * width + z * (width * height)
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
