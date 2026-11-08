#include <mge/gpu/GpuNoiseField.h>

#include <mge/core/Log.h>
#include <mge/gpu/Pipeline.h>

#include <shaders/noise_comp.h>

#include <array>
#include <cstring>

namespace mge {

    namespace {
        // Layout std140 para los uniformes del shader noise.comp (64 bytes total)
        struct UniformParams {
            glm::vec3 origin{0.0f};
            float pad0 = 0.0f;
            glm::vec3 scale{1.0f};
            float pad1 = 0.0f;
            std::uint32_t width = 0;
            std::uint32_t height = 0;
            std::uint32_t depth = 0;
            std::int32_t octaves = 5;
            float frequency = 1.0f;
            float lacunarity = 2.0f;
            float gain = 0.5f;
            float pad2 = 0.0f;
        };
        static_assert(sizeof(UniformParams) == 64, "UniformParams debe tener 64 bytes para std140");
    } // namespace

    GpuNoiseField::GpuNoiseField(GpuDevice& device, const Noise& noise) : m_device(&device) {
        initPipeline();
        setNoise(noise);
    }

    GpuNoiseField::GpuNoiseField(GpuDevice& device, std::uint32_t seed)
        : GpuNoiseField(device, Noise{seed}) {}

    void GpuNoiseField::initPipeline() {
        if (!m_device) return;
        ComputePipelineDesc desc;
        desc.bytecode = shaders::kNoiseComp;
        desc.threadcountX = 4;
        desc.threadcountY = 4;
        desc.threadcountZ = 4;
        desc.readonlyStorageBuffers = 1;  // Set 0, binding 0
        desc.readwriteStorageBuffers = 1; // Set 1, binding 0
        desc.uniformBuffers = 1;          // Set 2, binding 0
        m_pipeline = m_device->createComputePipeline(desc);
        if (!m_pipeline) {
            MGE_LOG_ERROR("Gpu", "No se pudo crear el pipeline de computo para GpuNoiseField");
        }
    }

    void GpuNoiseField::setNoise(const Noise& noise) {
        if (!m_device) return;
        std::array<std::uint32_t, 256> perm32;
        const auto& perm8 = noise.permutationTable();
        for (std::size_t i = 0; i < 256; ++i) {
            perm32[i] = perm8[i];
        }
        const auto byteSize = static_cast<std::uint32_t>(sizeof(perm32));
        if (!m_permBuffer) {
            m_permBuffer = m_device->createBuffer(SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ, byteSize);
        }
        m_device->uploadToBuffer(m_permBuffer, perm32.data(), byteSize);
    }

    void GpuNoiseField::generate(CommandBuffer& cmd, const GpuNoiseFieldDesc& desc) {
        if (!m_device || !m_pipeline || !m_permBuffer) return;
        m_desc = desc;
        const std::size_t totalFloats = totalElements();
        if (totalFloats == 0) return;

        const auto totalBytes = static_cast<std::uint32_t>(totalFloats * sizeof(float));
        if (!m_densityBuffer || m_allocatedBytes < totalBytes) {
            m_densityBuffer = m_device->createBuffer(SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE, totalBytes);
            m_allocatedBytes = totalBytes;
        }

        UniformParams params;
        params.origin = desc.origin;
        params.scale = desc.scale;
        params.width = desc.width;
        params.height = desc.height;
        params.depth = desc.depth;
        params.octaves = desc.fractal.octaves;
        params.frequency = desc.fractal.frequency;
        params.lacunarity = desc.fractal.lacunarity;
        params.gain = desc.fractal.gain;

        cmd.pushComputeUniform(0, params);

        ComputePass pass = cmd.beginComputePass(m_densityBuffer);
        pass.bindPipeline(m_pipeline);
        pass.bindStorageBuffers(0, m_permBuffer);

        const std::uint32_t groupX = (desc.width + 3) / 4;
        const std::uint32_t groupY = (desc.height + 3) / 4;
        const std::uint32_t groupZ = (desc.depth + 3) / 4;
        pass.dispatch(groupX, groupY, groupZ);
    }

    void GpuNoiseField::generate(const GpuNoiseFieldDesc& desc) {
        if (!m_device) return;
        CommandBuffer cmd = m_device->acquireCommandBuffer();
        generate(cmd, desc);
        cmd.submitAndWait();
    }

    void GpuNoiseField::generate(std::uint32_t width, std::uint32_t height, std::uint32_t depth,
                                 const glm::vec3& origin, const glm::vec3& scale,
                                 const FractalSettings& fractal) {
        GpuNoiseFieldDesc d;
        d.width = width;
        d.height = height;
        d.depth = depth;
        d.origin = origin;
        d.scale = scale;
        d.fractal = fractal;
        generate(d);
    }

    void GpuNoiseField::generate(std::uint32_t width, std::uint32_t height, std::uint32_t depth,
                                 const glm::vec3& origin, float uniformScale,
                                 const FractalSettings& fractal) {
        generate(width, height, depth, origin, glm::vec3(uniformScale), fractal);
    }

    std::vector<float> GpuNoiseField::download() const {
        if (!m_device || !m_densityBuffer) return {};
        const std::size_t totalFloats = totalElements();
        const auto totalBytes = static_cast<std::uint32_t>(totalFloats * sizeof(float));
        const std::vector<std::uint8_t> bytes = m_device->downloadBuffer(m_densityBuffer, totalBytes);
        if (bytes.size() != totalBytes) return {};

        std::vector<float> result(totalFloats);
        std::memcpy(result.data(), bytes.data(), totalBytes);
        return result;
    }

} // namespace mge
