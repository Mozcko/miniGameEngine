#include <mge/graphics/SdfRenderer.h>

#include <mge/core/Log.h>
#include <mge/graphics/MeshRenderer.h>

#include <shaders/sky_vert.h>

namespace mge {

    namespace {
        // Uniforms del fragment shader para iluminación (set 3, binding 0, coincidente con lighting.glsl)
        struct LightingUniform {
            glm::vec4 toSun;
            glm::vec4 sunColor;
            glm::vec4 skyAmbient;
            glm::vec4 groundAmbient;
            glm::vec4 fogColor;
            glm::vec4 fogParams;
            glm::vec4 cameraPosition;
        };

        // Uniforms del fragment shader para SDF raymarching (set 3, binding 1, coincidente con sdf_pass.glsl)
        struct SdfUniforms {
            glm::mat4 viewProjection;
            glm::mat4 inverseViewProjection;
            glm::vec4 cameraPositionAndTime;
            glm::vec4 renderSettings; // x: maxDistance, y: maxSteps
            glm::vec4 params[4];
        };
    } // namespace

    SdfRenderer::SdfRenderer(GpuDevice& device, SDL_GPUTextureFormat colorFormat,
                             SDL_GPUTextureFormat depthFormat, const ShaderBytecode& fragment) {
        ShaderResources vsResources{};
        ShaderResources fsResources{};
        fsResources.uniformBuffers = 2; // Slot 0: Lighting, Slot 1: SdfUniforms

        const GpuShader vs = device.createShader(shaders::kSkyVert, vsResources);
        const GpuShader fs = device.createShader(fragment, fsResources);

        GraphicsPipelineDesc desc;
        desc.vertexShader = &vs;
        desc.fragmentShader = &fs;
        desc.colorFormat = colorFormat;
        desc.depthFormat = depthFormat;
        desc.depthTest = true;
        desc.depthWrite = true;
        desc.compareOp = SDL_GPU_COMPAREOP_LESS;
        desc.gpuVertexInput = false;
        desc.cullMode = SDL_GPU_CULLMODE_NONE;
        desc.blend = BlendMode::None;

        m_pipeline = device.createGraphicsPipeline(desc);
        if (!valid()) {
            MGE_LOG_ERROR("Graphics", "No se pudo crear el SdfRenderer");
        }
    }

    void SdfRenderer::draw(MeshRenderer& renderer, const SdfFrame& frame, const Lighting& lighting) {
        if (!valid() || !renderer.currentPass() || !renderer.currentCommandBuffer()) return;

        SDL_GPURenderPass* pass = renderer.currentPass();
        CommandBuffer* cmd = renderer.currentCommandBuffer();

        SDL_BindGPUGraphicsPipeline(pass, m_pipeline.get());
        renderer.resetBoundPipeline();

        // Slot 0: Lighting (layout std140 idéntico a lighting.glsl)
        const LightingUniform lightingUniform{
            glm::vec4(-glm::normalize(lighting.sunDirection), 0.0f),
            glm::vec4(lighting.sunColor, 0.0f),
            glm::vec4(lighting.skyAmbient, 0.0f),
            glm::vec4(lighting.groundAmbient, 0.0f),
            glm::vec4(lighting.fogColor, 0.0f),
            glm::vec4(lighting.fogStart, lighting.fogEnd, lighting.fog ? 1.0f : 0.0f, 0.0f),
            glm::vec4(frame.cameraPosition, 1.0f)};
        SDL_PushGPUFragmentUniformData(cmd->get(), 0, &lightingUniform, sizeof(lightingUniform));

        // Slot 1: SdfUniforms (layout std140 idéntico a sdf_pass.glsl)
        const SdfUniforms sdfUniforms{frame.viewProjection,
                                      frame.inverseViewProjection,
                                      glm::vec4(frame.cameraPosition, frame.time),
                                      glm::vec4(maxDistance, static_cast<float>(maxSteps), 0.0f, 0.0f),
                                      {frame.params[0], frame.params[1], frame.params[2], frame.params[3]}};
        SDL_PushGPUFragmentUniformData(cmd->get(), 1, &sdfUniforms, sizeof(sdfUniforms));

        SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
    }

} // namespace mge
