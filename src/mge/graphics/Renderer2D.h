#pragma once

#include <mge/gpu/CommandBuffer.h>
#include <mge/gpu/GpuDevice.h>
#include <mge/graphics/Batch2D.h>
#include <mge/graphics/Texture2D.h>

#include <cstdint>
#include <string_view>

namespace mge {

    // Una fuente con su atlas ya en la GPU. Solo se puede mover; liberarla antes que el device
    struct GpuFont {
        Font font;
        Texture2D texture;

        static GpuFont create(GpuDevice& device, Font font);
        bool valid() const { return font.valid() && static_cast<bool>(texture); }
    };

    // Renderer 2D inmediato en píxeles (#42): UI, texto, overlays. Un buffer de vértices por frame y un
    // draw call por comando del lote (cambio de textura o recorte). Uso por frame, después de la escena 3D:
    //   ui.begin(frame.width, frame.height, escala);   // escala: píxeles físicos por píxel lógico (DPI)
    //   ui.rect(...); ui.text({10, 10}, "FPS: 60", color); ...
    //   ui.flush(*frame.cmd, frame.swapchain);         // dibuja encima (no limpia la imagen)
    // Coordenadas en píxeles lógicos: origen arriba a la izquierda, y hacia abajo; size() = tamaño lógico.
    class Renderer2D : public Batch2D {
    public:
        Renderer2D(GpuDevice& device, SDL_GPUTextureFormat colorFormat);

        bool valid() const { return m_pipeline && m_nearest && m_linear && m_white && m_builtin.valid(); }

        // Empieza un frame: vacía el lote. width/height en píxeles físicos; scale > 0
        void begin(std::uint32_t width, std::uint32_t height, float scale = 1.0f);
        glm::vec2 size() const { return m_logicalSize; }
        float scale() const { return m_scale; }

        // Texto con la fuente 8x8 embebida (escala entera = píxeles nítidos) o con otra fuente
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

        // Sube el lote y lo dibuja sobre target (carga lo que ya tiene). Después hay que volver a begin()
        void flush(CommandBuffer& cmd, SDL_GPUTexture* target);

        // Del último flush
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
        std::uint32_t m_capacity = 0; // Vértices
        std::uint32_t m_width = 0;
        std::uint32_t m_height = 0;
        float m_scale = 1.0f;
        glm::vec2 m_logicalSize{0.0f};
        std::uint32_t m_drawCalls = 0;
        std::uint32_t m_vertexCount = 0;
    };
}
