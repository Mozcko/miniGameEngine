#include <mge/graphics/MeshRenderer.h>

#include <mge/core/Log.h>

#include <shaders/debug_lines_frag.h>
#include <shaders/debug_lines_vert.h>
#include <shaders/mesh_cutout_frag.h>
#include <shaders/mesh_frag.h>
#include <shaders/mesh_vert.h>

namespace mge {

    namespace {
        struct Transforms {
            glm::mat4 viewProjection;
            glm::mat4 model;
            glm::vec4 material; // x: escala del detalle, y: intensidad
        };

        // Igual que el bloque Lighting de mesh.frag (todo vec4: sin sorpresas de alineación std140)
        struct LightingUniform {
            glm::vec4 toSun;
            glm::vec4 sunColor;
            glm::vec4 skyAmbient;
            glm::vec4 groundAmbient;
            glm::vec4 fogColor;
            glm::vec4 fogParams;
            glm::vec4 cameraPosition;
        };

        LightingUniform toUniform(const Lighting& l, const glm::vec3& cameraPosition) {
            return {glm::vec4(-glm::normalize(l.sunDirection), 0.0f),
                    glm::vec4(l.sunColor, 0.0f),
                    glm::vec4(l.skyAmbient, 0.0f),
                    glm::vec4(l.groundAmbient, 0.0f),
                    glm::vec4(l.fogColor, 0.0f),
                    glm::vec4(l.fogStart, l.fogEnd, l.fog ? 1.0f : 0.0f, 0.0f),
                    glm::vec4(cameraPosition, 1.0f)};
        }
    }

    MeshRenderer::MeshRenderer(GpuDevice& device, SDL_GPUTextureFormat colorFormat) : m_device(device) {
        ShaderResources vertexResources;
        vertexResources.uniformBuffers = 1; // Transforms
        ShaderResources fragmentResources;
        fragmentResources.samplers = 1;       // albedo
        fragmentResources.uniformBuffers = 1; // Lighting
        const GpuShader vertex = device.createShader(shaders::kMeshVert, vertexResources);
        const GpuShader fragment = device.createShader(shaders::kMeshFrag, fragmentResources);
        const GpuShader cutoutFragment = device.createShader(shaders::kMeshCutoutFrag, fragmentResources);

        GraphicsPipelineDesc desc;
        desc.vertexShader = &vertex;
        desc.fragmentShader = &fragment;
        desc.colorFormat = colorFormat;
        desc.depthFormat = device.depthFormat();
        m_opaque = device.createGraphicsPipeline(desc);

        // Cutout: descarta alfa < 0.5 y escribe profundidad. Con back-face culling, como un cubo opaco con
        // agujeros (las plantas en cruz ya traen sus dos caras como quads separados)
        desc.fragmentShader = &cutoutFragment;
        m_cutout = device.createGraphicsPipeline(desc);
        desc.fragmentShader = &fragment;

        desc.blend = BlendMode::Alpha;
        desc.depthWrite = false;
        m_transparent = device.createGraphicsPipeline(desc);

        // Líneas de depuración: lista de líneas, sin culling, con prueba de profundidad pero sin escribirla
        ShaderResources lineVertexResources;
        lineVertexResources.uniformBuffers = 1;
        const GpuShader lineVertex = device.createShader(shaders::kDebugLinesVert, lineVertexResources);
        const GpuShader lineFragment = device.createShader(shaders::kDebugLinesFrag);
        GraphicsPipelineDesc lines;
        lines.vertexShader = &lineVertex;
        lines.fragmentShader = &lineFragment;
        lines.colorFormat = colorFormat;
        lines.depthFormat = device.depthFormat();
        lines.depthWrite = false;
        lines.primitive = SDL_GPU_PRIMITIVETYPE_LINELIST;
        lines.cullMode = SDL_GPU_CULLMODE_NONE;
        lines.blend = BlendMode::Alpha;
        m_lines = device.createGraphicsPipeline(lines);

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
        m_stats = {};
        m_lightingDirty = true; // Command buffer nuevo: hay que volver a subir la luz
        return static_cast<bool>(m_pass);
    }

    void MeshRenderer::draw(const GpuMesh& mesh, const glm::mat4& model, const Texture2D* texture,
                            MeshPass pass, const MeshMaterial& material) {
        if (!m_pass || mesh.empty()) return;
        SDL_GPUGraphicsPipeline* pipeline = pass == MeshPass::Opaque   ? m_opaque.get()
                                            : pass == MeshPass::Cutout ? m_cutout.get()
                                                                       : m_transparent.get();
        if (pipeline != m_boundPipeline) {
            SDL_BindGPUGraphicsPipeline(m_pass.get(), pipeline);
            m_boundPipeline = pipeline;
        }
        const Transforms transforms{m_viewProjection, model,
                                    glm::vec4(material.detailScale, material.detailStrength, 0.0f, 0.0f)};
        SDL_PushGPUVertexUniformData(m_cmd->get(), 0, &transforms, sizeof(transforms));
        if (m_lightingDirty) {
            const LightingUniform lighting = toUniform(m_lighting, m_cameraPosition);
            SDL_PushGPUFragmentUniformData(m_cmd->get(), 0, &lighting, sizeof(lighting));
            m_lightingDirty = false;
        }

        const Texture2D& albedo = texture && *texture ? *texture : m_white;
        SDL_GPUTextureSamplerBinding binding{albedo.gpu().get(), m_sampler.get()};
        SDL_BindGPUFragmentSamplers(m_pass.get(), 0, &binding, 1);

        SDL_GPUBufferBinding vertices{mesh.vertices.get(), 0};
        SDL_GPUBufferBinding indices{mesh.indices.get(), 0};
        SDL_BindGPUVertexBuffers(m_pass.get(), 0, &vertices, 1);
        SDL_BindGPUIndexBuffer(m_pass.get(), &indices, SDL_GPU_INDEXELEMENTSIZE_32BIT);
        SDL_DrawGPUIndexedPrimitives(m_pass.get(), mesh.indexCount, 1, 0, 0, 0);
        const auto index = static_cast<std::size_t>(pass);
        m_stats.drawCalls[index]++;
        m_stats.triangles[index] += mesh.indexCount / 3;
    }

    void MeshRenderer::submit(const RenderQueue& queue) {
        for (const DrawItem& item : queue.items())
            draw(*item.mesh, item.model, item.texture, item.pass, item.material);
    }

    void MeshRenderer::drawLines(const GpuBuffer& vertices, std::uint32_t vertexCount) {
        if (!m_pass || !vertices || vertexCount == 0) return;
        SDL_BindGPUGraphicsPipeline(m_pass.get(), m_lines.get());
        m_boundPipeline = m_lines.get();
        SDL_PushGPUVertexUniformData(m_cmd->get(), 0, &m_viewProjection, sizeof(m_viewProjection));
        SDL_GPUBufferBinding binding{vertices.get(), 0};
        SDL_BindGPUVertexBuffers(m_pass.get(), 0, &binding, 1);
        SDL_DrawGPUPrimitives(m_pass.get(), vertexCount, 1, 0, 0);
        // La próxima malla vuelve a subir sus uniforms (este pipeline usó el slot 0 del vértice con otro
        // layout)
    }

    void MeshRenderer::end() {
        m_pass.end();
        m_cmd = nullptr;
        m_boundPipeline = nullptr;
    }
}
