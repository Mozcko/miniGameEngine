#pragma once

#include <mge/gpu/GpuMesh.h>
#include <mge/procgen/Font.h>

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>

namespace mge {

    class Texture2D;

    // Rectángulo en píxeles de la UI: origen arriba a la izquierda, y hacia abajo
    struct Rect2D {
        glm::vec2 pos{0.0f};
        glm::vec2 size{0.0f};

        glm::vec2 max() const { return pos + size; }
        bool empty() const { return size.x <= 0.0f || size.y <= 0.0f; }
        // Intersección (vacía si no se tocan)
        Rect2D intersect(const Rect2D& other) const;
        bool operator==(const Rect2D& o) const { return pos == o.pos && size == o.size; }
    };

    // Un tramo de vértices que se dibuja con la misma textura, filtro y recorte (un draw call)
    struct Draw2DCommand {
        const Texture2D* texture = nullptr; // nullptr = blanca (solo color)
        bool nearest = true;                // Muestreo sin filtrar (pixel art, fuente 8x8)
        std::optional<Rect2D> clip;         // Sin valor = toda la pantalla
        std::uint32_t firstVertex = 0;
        std::uint32_t vertexCount = 0;
    };

    // Lote 2D inmediato (#42), armado en la CPU (se testea sin GPU). En píxeles lógicos de la UI: origen
    // arriba a la izquierda, y hacia abajo. Cada primitiva son triángulos sueltos (6 vértices por quad); un
    // comando nuevo empieza solo cuando cambia la textura, el filtro o el recorte. Lo que no tiene textura
    // (rect, line) no corta el lote: lleva UV kNoTexture y el shader usa blanco, así cientos de elementos
    // salen en uno o pocos draw calls. Renderer2D le suma la GPU.
    class Batch2D {
    public:
        // UV de los quads sin textura (ui.frag usa blanco)
        static constexpr float kNoTexture = -1.0f;

        void clear();

        void rect(const glm::vec2& pos, const glm::vec2& size, const glm::vec4& color);
        // Borde hacia adentro del rectángulo
        void rectOutline(const glm::vec2& pos, const glm::vec2& size, const glm::vec4& color,
                         float thickness = 1.0f);
        void line(const glm::vec2& a, const glm::vec2& b, const glm::vec4& color, float thickness = 1.0f);
        // uvMin/uvMax en la convención de Texture2D (v hacia arriba: (0, 0) = abajo a la izquierda de la
        // imagen)
        void image(const Texture2D* texture, const glm::vec2& pos, const glm::vec2& size,
                   const glm::vec2& uvMin = glm::vec2(0.0f), const glm::vec2& uvMax = glm::vec2(1.0f),
                   const glm::vec4& tint = glm::vec4(1.0f), bool nearest = false);
        // Texto con font (su atlas ya subido a atlasTexture). pos = arriba a la izquierda de la primera
        // línea;
        // '\n' baja una línea. Devuelve el tamaño ocupado (como Font::measure)
        glm::vec2 text(const Font& font, const Texture2D* atlasTexture, const glm::vec2& pos,
                       std::string_view text, const glm::vec4& color, float scale = 1.0f);

        // Recorte: lo de afuera no se dibuja. Se anidan (intersección)
        void pushClip(const Rect2D& rect);
        void popClip();

        const std::vector<GpuVertex>& vertices() const { return m_vertices; }
        const std::vector<Draw2DCommand>& commands() const { return m_commands; }
        std::size_t quadCount() const { return m_vertices.size() / 6; }

    private:
        // Quad alineado a los ejes (p0 arriba a la izquierda) y quad con 4 esquinas libres. textured = false:
        // sin textura (no corta el lote)
        void quad(bool textured, const Texture2D* texture, bool nearest, const glm::vec2& p0,
                  const glm::vec2& p1, const glm::vec2& uv0, const glm::vec2& uv1, const glm::vec4& color);
        void quadCorners(bool textured, const Texture2D* texture, bool nearest, const glm::vec2 corners[4],
                         const glm::vec2 uvs[4], const glm::vec4& color);
        Draw2DCommand& commandFor(bool textured, const Texture2D* texture, bool nearest);

        std::vector<GpuVertex> m_vertices;
        std::vector<Draw2DCommand> m_commands;
        std::vector<Rect2D> m_clipStack;
    };
}
