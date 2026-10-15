#include <mge/graphics/MeshRenderer.h>

#include <mge/core/Log.h>

#include <shaders/mesh_frag.h>
#include <shaders/mesh_vert.h>

namespace mge {

    namespace {
        struct Transforms {
            glm::mat4 viewProjection;
            glm::mat4 model;
        };
    }

    MeshRenderer::MeshRenderer(GpuDevice& device, SDL_GPUTextureFormat colorFormat) : m_device(device) {
        ShaderResources vertexResources;
        vertexResources.uniformBuffers = 1; // Transforms
        ShaderResources fragmentResources;
        fragmentResources.samplers = 1; // albedo
        const GpuShader vertex = device.createShader(shaders::kMeshVert, vertexResources);
        const GpuShader fragment = device.createShader(shaders::kMeshFrag, fragmentResources);

        GraphicsPipelineDesc desc;
        desc.vertexShader = &vertex;
        desc.fragmentShader = &fragment;
        desc.colorFormat = colorFormat;
        desc.depthFormat = device.depthFormat();
        m_opaque = device.createGraphicsPipeline(desc);

        desc.blend = BlendMode::Alpha;
        desc.depthWrite = false;
        m_transparent = device.createGraphicsPipeline(desc);

        // Píxeles nítidos (texturas tipo pixel art) y sin repetir fuera de [0, 1], como el renderer GL
        SDL_GPUSamplerCreateInfo sampler{};
        sampler.min_filter = SDL_GPU_FILTER_NEAREST;
        sampler.mag_filter = SDL_GPU_FILTER_NEAREST;
        sampler.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
        sampler.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        sampler.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        sampler.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        m_sampler = device.createSampler(sampler);
        m_white = Texture2D::white(device);
        if (!valid()) MGE_LOG_ERROR("Graphics", "No se pudo crear el MeshRenderer");
    }

    bool MeshRenderer::begin(CommandBuffer& cmd, SDL_GPUTexture* target, std::uint32_t width,
                             std::uint32_t height, const glm::vec4& clearColor) {
        if (!valid() || !target || width == 0 || height == 0) return false;
        if (!m_depth || m_depthWidth != width || m_depthHeight != height) {
            m_depth = m_device.createDepthTarget(width, height);
            m_depthWidth = width;
            m_depthHeight = height;
        }
        ColorTarget color;
        color.texture = target;
        color.clearColor = clearColor;
        DepthTarget depth;
        depth.texture = m_depth.get();
        m_pass = cmd.beginRenderPass(color, &depth);
        m_cmd = &cmd;
        m_boundPipeline = nullptr;
        m_drawCalls = 0;
        m_triangles = 0;
        return static_cast<bool>(m_pass);
    }

    void MeshRenderer::draw(const GpuMesh& mesh, const glm::mat4& model, const Texture2D* texture,
                            MeshPass pass) {
        if (!m_pass || mesh.empty()) return;
        SDL_GPUGraphicsPipeline* pipeline = pass == MeshPass::Opaque ? m_opaque.get() : m_transparent.get();
        if (pipeline != m_boundPipeline) {
            SDL_BindGPUGraphicsPipeline(m_pass.get(), pipeline);
            m_boundPipeline = pipeline;
        }
        const Transforms transforms{m_viewProjection, model};
        SDL_PushGPUVertexUniformData(m_cmd->get(), 0, &transforms, sizeof(transforms));

        const Texture2D& albedo = texture && *texture ? *texture : m_white;
        SDL_GPUTextureSamplerBinding binding{albedo.gpu().get(), m_sampler.get()};
        SDL_BindGPUFragmentSamplers(m_pass.get(), 0, &binding, 1);

        SDL_GPUBufferBinding vertices{mesh.vertices.get(), 0};
        SDL_GPUBufferBinding indices{mesh.indices.get(), 0};
        SDL_BindGPUVertexBuffers(m_pass.get(), 0, &vertices, 1);
        SDL_BindGPUIndexBuffer(m_pass.get(), &indices, SDL_GPU_INDEXELEMENTSIZE_32BIT);
        SDL_DrawGPUIndexedPrimitives(m_pass.get(), mesh.indexCount, 1, 0, 0, 0);
        m_drawCalls++;
        m_triangles += mesh.indexCount / 3;
    }

    void MeshRenderer::end() {
        m_pass.end();
        m_cmd = nullptr;
        m_boundPipeline = nullptr;
    }
}
