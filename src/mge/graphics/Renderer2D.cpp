#include <mge/graphics/Renderer2D.h>

#include <mge/core/Log.h>

#include <shaders/ui_frag.h>
#include <shaders/ui_vert.h>

#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

namespace mge {

    GpuFont GpuFont::create(GpuDevice& device, Font font) {
        GpuFont result;
        if (!font.valid()) return result;
        const Image& atlas = font.atlas();
        result.texture = Texture2D::fromPixels(device, atlas.width(), atlas.height(), atlas.data());
        result.font = std::move(font);
        return result;
    }

    namespace {
        GpuSampler makeSampler(GpuDevice& device, SDL_GPUFilter filter) {
            SDL_GPUSamplerCreateInfo info{};
            info.min_filter = filter;
            info.mag_filter = filter;
            info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
            info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
            info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
            info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
            return device.createSampler(info);
        }
    }

    Renderer2D::Renderer2D(GpuDevice& device, SDL_GPUTextureFormat colorFormat) : m_device(device) {
        ShaderResources vertexResources;
        vertexResources.uniformBuffers = 1; // Proyección
        ShaderResources fragmentResources;
        fragmentResources.samplers = 1;
        const GpuShader vertex = device.createShader(shaders::kUiVert, vertexResources);
        const GpuShader fragment = device.createShader(shaders::kUiFrag, fragmentResources);

        // Sin profundidad ni culling, con mezcla alfa: se dibuja en orden, encima de la escena
        GraphicsPipelineDesc desc;
        desc.vertexShader = &vertex;
        desc.fragmentShader = &fragment;
        desc.colorFormat = colorFormat;
        desc.cullMode = SDL_GPU_CULLMODE_NONE;
        desc.blend = BlendMode::Alpha;
        m_pipeline = device.createGraphicsPipeline(desc);

        m_nearest = makeSampler(device, SDL_GPU_FILTER_NEAREST);
        m_linear = makeSampler(device, SDL_GPU_FILTER_LINEAR);
        m_white = Texture2D::white(device);
        m_builtin = GpuFont::create(device, Font::builtin());
        if (!valid()) MGE_LOG_ERROR("Graphics", "No se pudo crear el Renderer2D");
    }

    void Renderer2D::begin(std::uint32_t width, std::uint32_t height, float scale) {
        clear();
        m_width = width;
        m_height = height;
        m_scale = scale > 0.0f ? scale : 1.0f;
        m_logicalSize = glm::vec2(float(width), float(height)) / m_scale;
    }

    void Renderer2D::flush(CommandBuffer& cmd, SDL_GPUTexture* target) {
        m_drawCalls = 0;
        m_vertexCount = 0;
        const auto count = static_cast<std::uint32_t>(vertices().size());
        if (!valid() || !target || count == 0 || m_width == 0 || m_height == 0) {
            clear();
            return;
        }
        if (count > m_capacity) {
            m_capacity = std::max(count, m_capacity * 2);
            m_buffer = m_device.createBuffer(SDL_GPU_BUFFERUSAGE_VERTEX,
                                             m_capacity * std::uint32_t(sizeof(GpuVertex)));
        }
        // cycle: si el frame anterior todavía usa el buffer, SDL da otro por detrás (sin pisarlo)
        if (!m_buffer || !m_device.uploadToBuffer(m_buffer, vertices().data(),
                                                  count * std::uint32_t(sizeof(GpuVertex)), true)) {
            clear();
            return;
        }

        ColorTarget color;
        color.texture = target;
        color.clear = false;
        RenderPass pass = cmd.beginRenderPass(color);
        if (!pass) {
            clear();
            return;
        }
        SDL_BindGPUGraphicsPipeline(pass.get(), m_pipeline.get());
        // Píxeles lógicos → NDC (SDL GPU: y hacia arriba): y = 0 arriba
        const glm::mat4 projection =
            glm::orthoRH_ZO(0.0f, m_logicalSize.x, m_logicalSize.y, 0.0f, -1.0f, 1.0f);
        SDL_PushGPUVertexUniformData(cmd.get(), 0, &projection, sizeof(projection));
        SDL_GPUBufferBinding binding{m_buffer.get(), 0};
        SDL_BindGPUVertexBuffers(pass.get(), 0, &binding, 1);

        for (const Draw2DCommand& command : commands()) {
            if (command.vertexCount == 0) continue;
            // Recorte en píxeles físicos, dentro de la imagen
            SDL_Rect scissor{0, 0, int(m_width), int(m_height)};
            if (command.clip) {
                const glm::vec2 lo = glm::clamp(glm::floor(command.clip->pos * m_scale), glm::vec2(0.0f),
                                                glm::vec2(float(m_width), float(m_height)));
                const glm::vec2 hi = glm::clamp(glm::ceil(command.clip->max() * m_scale), lo,
                                                glm::vec2(float(m_width), float(m_height)));
                scissor = {int(lo.x), int(lo.y), int(hi.x - lo.x), int(hi.y - lo.y)};
                if (scissor.w == 0 || scissor.h == 0) continue;
            }
            SDL_SetGPUScissor(pass.get(), &scissor);
            const Texture2D& texture = command.texture && *command.texture ? *command.texture : m_white;
            SDL_GPUTextureSamplerBinding sampler{texture.gpu().get(),
                                                 command.nearest ? m_nearest.get() : m_linear.get()};
            SDL_BindGPUFragmentSamplers(pass.get(), 0, &sampler, 1);
            SDL_DrawGPUPrimitives(pass.get(), command.vertexCount, 1, command.firstVertex, 0);
            m_drawCalls++;
        }
        m_vertexCount = count;
        pass.end();
        clear();
    }
}
