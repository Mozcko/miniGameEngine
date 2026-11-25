#pragma once

#include <mge/gpu/CommandBuffer.h>
#include <mge/gpu/GpuDevice.h>
#include <mge/graphics/Batch2D.h>
#include <mge/graphics/Texture2D.h>

#include <cstdint>
#include <string_view>

namespace mge {

    // A font with its atlas already on the GPU. Move-only; release it before the device
    struct GpuFont {
        Font font;
        Texture2D texture;

        static GpuFont create(GpuDevice& device, Font font);
        bool valid() const { return font.valid() && static_cast<bool>(texture); }
    };

    // Immediate-mode 2D renderer in pixels (#42): UI, text, overlays. One vertex buffer per frame and one
    // draw call per batch command (texture or scissor change). Per-frame usage, after the 3D scene:
    //   ui.begin(frame.width, frame.height, scale);    // scale: physical pixels per logical pixel (DPI)
    //   ui.rect(...); ui.text({10, 10}, "FPS: 60", color); ...
    //   ui.flush(*frame.cmd, frame.swapchain);         // draws on top (does not clear the image)
    // Coordinates in logical pixels: origin at top-left, y pointing down; size() = logical size.
    class Renderer2D : public Batch2D {
    public:
        Renderer2D(GpuDevice& device, SDL_GPUTextureFormat colorFormat);

        bool valid() const { return m_pipeline && m_nearest && m_linear && m_white && m_builtin.valid(); }

        // Starts a frame: clears the batch. width/height in physical pixels; scale > 0
        void begin(std::uint32_t width, std::uint32_t height, float scale = 1.0f);
        glm::vec2 size() const { return m_logicalSize; }
        float scale() const { return m_scale; }

        // Text with the embedded 8x8 font (integer scale = crisp pixels) or another font
        using Batch2D::text;
        glm::vec2 text(const glm::vec2& pos, std::string_view str, const glm::vec4& color,
                       float scale = 1.0f) {
            return text(m_builtin, pos, str, color, scale);
        }
        glm::vec2 text(const GpuFont& font, const glm::vec2& pos, std::string_view str,
                       const glm::vec4& color, float scale = 1.0f) {
            return Batch2D::text(font.font, &font.texture, pos, str, color, scale);
        }
        const GpuFont& builtinFont() const { return m_builtin; }

        // Uploads the batch and draws it onto target (loads existing contents). Must call begin() again
        // afterwards
        void flush(CommandBuffer& cmd, SDL_GPUTexture* target);

        // From the last flush
        std::uint32_t drawCalls() const { return m_drawCalls; }
        std::uint32_t vertexCount() const { return m_vertexCount; }

    private:
        GpuDevice& m_device;
        GpuGraphicsPipeline m_pipeline;
        GpuSampler m_nearest;
        GpuSampler m_linear;
        Texture2D m_white;
        GpuFont m_builtin;
        GpuBuffer m_buffer;
        std::uint32_t m_capacity = 0; // Vertices
        std::uint32_t m_width = 0;
        std::uint32_t m_height = 0;
        float m_scale = 1.0f;
        glm::vec2 m_logicalSize{0.0f};
        std::uint32_t m_drawCalls = 0;
        std::uint32_t m_vertexCount = 0;
    };
}
