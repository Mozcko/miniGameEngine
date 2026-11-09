#include <mge/graphics/Sky.h>

#include <mge/core/Log.h>
#include <mge/graphics/MeshRenderer.h>

#include <shaders/sky_frag.h>
#include <shaders/sky_vert.h>

namespace mge {

    namespace {
        // Uniforms del fragment shader sky.frag (set 3, binding 0)
        struct SkyUniforms {
            glm::mat4 inverseViewProjection;
            glm::vec4 cameraPosition;
            glm::vec4 zenithColor;
            glm::vec4 horizonColor;
            glm::vec4 groundColor;
            glm::vec4 sunDirection;
            glm::vec4 sunColor;
            glm::vec4 sunParams; // x: sunSize, y: sunGlow, z: starsIntensity, w: starsSeed
        };
    } // namespace

    SkyRenderer::SkyRenderer(GpuDevice& device, SDL_GPUTextureFormat colorFormat,
                             SDL_GPUTextureFormat depthFormat) {
        ShaderResources vsResources{};
        ShaderResources fsResources{};
        fsResources.uniformBuffers = 1;

        const GpuShader vs = device.createShader(shaders::kSkyVert, vsResources);
        const GpuShader fs = device.createShader(shaders::kSkyFrag, fsResources);

        GraphicsPipelineDesc desc;
        desc.vertexShader = &vs;
        desc.fragmentShader = &fs;
        desc.colorFormat = colorFormat;
        desc.depthFormat = depthFormat;
        desc.depthTest = true;
        desc.depthWrite = false;
        desc.compareOp = SDL_GPU_COMPAREOP_ALWAYS;
        desc.gpuVertexInput = false;
        desc.cullMode = SDL_GPU_CULLMODE_NONE;
        desc.blend = BlendMode::None;

        m_pipeline = device.createGraphicsPipeline(desc);
        if (!valid()) {
            MGE_LOG_ERROR("Graphics", "No se pudo crear el SkyRenderer");
        }
    }

    void SkyRenderer::draw(MeshRenderer& renderer, const glm::mat4& inverseViewProjection,
                           const glm::vec3& cameraPosition, const SkySettings& sky) {
        if (!valid() || !renderer.currentPass() || !renderer.currentCommandBuffer()) return;

        SDL_GPURenderPass* pass = renderer.currentPass();
        CommandBuffer* cmd = renderer.currentCommandBuffer();

        SDL_BindGPUGraphicsPipeline(pass, m_pipeline.get());
        renderer.resetBoundPipeline();

        const SkyUniforms uniforms{
            inverseViewProjection,
            glm::vec4(cameraPosition, 1.0f),
            glm::vec4(sky.zenithColor, 0.0f),
            glm::vec4(sky.horizonColor, 0.0f),
            glm::vec4(sky.groundColor, 0.0f),
            glm::vec4(sky.sunDirection, 0.0f),
            glm::vec4(sky.sunColor, 0.0f),
            glm::vec4(sky.sunSize, sky.sunGlow, sky.starsIntensity, static_cast<float>(sky.starsSeed))};
        SDL_PushGPUFragmentUniformData(cmd->get(), 0, &uniforms, sizeof(uniforms));
        SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
    }

} // namespace mge
