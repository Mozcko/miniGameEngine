#include <mge/graphics/Batch2D.h>

#include <algorithm>
#include <cmath>

namespace mge {

    Rect2D Rect2D::intersect(const Rect2D& other) const {
        const glm::vec2 lo = glm::max(pos, other.pos);
        const glm::vec2 hi = glm::min(max(), other.max());
        return {lo, glm::max(hi - lo, glm::vec2(0.0f))};
    }

    void Batch2D::clear() {
        m_vertices.clear();
        m_commands.clear();
        m_clipStack.clear();
    }

    Draw2DCommand& Batch2D::commandFor(bool textured, const Texture2D* texture, bool nearest) {
        const std::optional<Rect2D> clip =
            m_clipStack.empty() ? std::nullopt : std::optional<Rect2D>(m_clipStack.back());
        if (!m_commands.empty()) {
            Draw2DCommand& last = m_commands.back();
            if (last.clip == clip) {
                if (!textured) return last; // Blanco por UV: entra en cualquier lote
                if (last.texture == texture && last.nearest == nearest) return last;
                if (!last.texture) { // Hasta ahora solo quads sin textura: el lote adopta esta
                    last.texture = texture;
                    last.nearest = nearest;
                    return last;
                }
            }
        }
        Draw2DCommand command;
        command.texture = textured ? texture : nullptr;
        command.nearest = nearest;
        command.clip = clip;
        command.firstVertex = static_cast<std::uint32_t>(m_vertices.size());
        m_commands.push_back(command);
        return m_commands.back();
    }

    void Batch2D::quadCorners(bool textured, const Texture2D* texture, bool nearest,
                              const glm::vec2 corners[4], const glm::vec2 uvs[4], const glm::vec4& color) {
        // Todo recortado: no gasta vértices
        if (!m_clipStack.empty() && m_clipStack.back().empty()) return;
        Draw2DCommand& command = commandFor(textured, texture, nearest);
        constexpr int kOrder[6] = {0, 1, 2, 0, 2, 3};
        for (int i : kOrder) {
            GpuVertex v{};
            v.position = glm::vec3(corners[i], 0.0f);
            v.color = color;
            v.uv = textured ? uvs[i] : glm::vec2(kNoTexture);
            m_vertices.push_back(v);
        }
        command.vertexCount += 6;
    }

    void Batch2D::quad(bool textured, const Texture2D* texture, bool nearest, const glm::vec2& p0,
                       const glm::vec2& p1, const glm::vec2& uv0, const glm::vec2& uv1,
                       const glm::vec4& color) {
        if (p1.x <= p0.x || p1.y <= p0.y) return;
        // Esquinas: arriba izq., arriba der., abajo der., abajo izq. (uv0 = arriba a la izquierda)
        const glm::vec2 corners[4] = {p0, {p1.x, p0.y}, p1, {p0.x, p1.y}};
        const glm::vec2 uvs[4] = {uv0, {uv1.x, uv0.y}, uv1, {uv0.x, uv1.y}};
        quadCorners(textured, texture, nearest, corners, uvs, color);
    }

    void Batch2D::rect(const glm::vec2& pos, const glm::vec2& size, const glm::vec4& color) {
        quad(false, nullptr, true, pos, pos + size, {}, {}, color);
    }

    void Batch2D::rectOutline(const glm::vec2& pos, const glm::vec2& size, const glm::vec4& color,
                              float thickness) {
        const float t = std::min({thickness, size.x * 0.5f, size.y * 0.5f});
        if (t <= 0.0f) return;
        rect(pos, {size.x, t}, color);                                        // Arriba
        rect({pos.x, pos.y + size.y - t}, {size.x, t}, color);                // Abajo
        rect({pos.x, pos.y + t}, {t, size.y - 2.0f * t}, color);              // Izquierda
        rect({pos.x + size.x - t, pos.y + t}, {t, size.y - 2.0f * t}, color); // Derecha
    }

    void Batch2D::line(const glm::vec2& a, const glm::vec2& b, const glm::vec4& color, float thickness) {
        const glm::vec2 d = b - a;
        const float length = glm::length(d);
        if (length <= 0.0f || thickness <= 0.0f) return;
        const glm::vec2 n = glm::vec2(-d.y, d.x) / length * (thickness * 0.5f);
        const glm::vec2 corners[4] = {a + n, b + n, b - n, a - n};
        const glm::vec2 uvs[4] = {};
        quadCorners(false, nullptr, true, corners, uvs, color);
    }

    void Batch2D::image(const Texture2D* texture, const glm::vec2& pos, const glm::vec2& size,
                        const glm::vec2& uvMin, const glm::vec2& uvMax, const glm::vec4& tint, bool nearest) {
        // Arriba a la izquierda en pantalla = (uMin, vMax) en la convención de Texture2D (v hacia arriba)
        quad(true, texture, nearest, pos, pos + size, {uvMin.x, uvMax.y}, {uvMax.x, uvMin.y}, tint);
    }

    glm::vec2 Batch2D::text(const Font& font, const Texture2D* atlasTexture, const glm::vec2& pos,
                            std::string_view str, const glm::vec4& color, float scale) {
        if (!font.valid() || str.empty()) return glm::vec2(0.0f);
        const glm::vec2 atlasSize(float(font.atlas().width()), float(font.atlas().height()));
        const bool nearest = font.isPixelFont();
        // Fuente de píxeles: el lápiz en píxeles enteros, para que no se deforme al muestrear sin filtro
        const glm::vec2 origin = nearest ? glm::round(pos) : pos;
        glm::vec2 pen = origin;
        float widest = 0.0f;
        int lines = 1;
        for (std::size_t i = 0; i < str.size();) {
            const char32_t c = nextCodepoint(str, i);
            if (c == U'\n') {
                widest = std::max(widest, pen.x - origin.x);
                pen.x = origin.x;
                pen.y += font.lineHeight() * scale;
                lines++;
                continue;
            }
            const Glyph& g = font.glyph(c);
            if (g.size.x > 0 && g.size.y > 0) {
                glm::vec2 p0 = pen + g.offset * scale;
                if (nearest) p0 = glm::round(p0);
                const glm::vec2 p1 = p0 + glm::vec2(g.size) * scale;
                // Atlas con fila 0 arriba; Texture2D lo guarda invertido (v = 1 arriba)
                const glm::vec2 a = glm::vec2(g.atlasPos) / atlasSize;
                const glm::vec2 b = glm::vec2(g.atlasPos + g.size) / atlasSize;
                quad(true, atlasTexture, nearest, p0, p1, {a.x, 1.0f - a.y}, {b.x, 1.0f - b.y}, color);
            }
            pen.x += g.advance * scale;
        }
        widest = std::max(widest, pen.x - origin.x);
        return {widest, float(lines) * font.lineHeight() * scale};
    }

    void Batch2D::pushClip(const Rect2D& rect) {
        m_clipStack.push_back(m_clipStack.empty() ? rect : m_clipStack.back().intersect(rect));
    }

    void Batch2D::popClip() {
        if (!m_clipStack.empty()) m_clipStack.pop_back();
    }
}
