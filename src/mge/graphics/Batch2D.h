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

    // Rectangle in UI pixels: origin at top-left, y pointing down
    struct Rect2D {
        glm::vec2 pos{0.0f};
        glm::vec2 size{0.0f};

        glm::vec2 max() const { return pos + size; }
        bool empty() const { return size.x <= 0.0f || size.y <= 0.0f; }
        // Intersection (empty if they do not overlap)
        Rect2D intersect(const Rect2D& other) const;
        bool operator==(const Rect2D& o) const { return pos == o.pos && size == o.size; }
    };

    // A span of vertices drawn with the same texture, filter, and clip rect (one draw call)
    struct Draw2DCommand {
        const Texture2D* texture = nullptr; // nullptr = white (color only)
        bool nearest = true;                // Unfiltered sampling (pixel art, 8x8 font)
        std::optional<Rect2D> clip;         // Empty = entire screen
        std::uint32_t firstVertex = 0;
        std::uint32_t vertexCount = 0;
    };

    // Immediate-mode 2D batch (#42), built on the CPU (testable without GPU). In logical UI pixels: origin
    // at top-left, y pointing down. Each primitive is loose triangles (6 vertices per quad); a new command
    // starts only when texture, filter, or clip changes. Untextured primitives (rect, line) do not break
    // the batch: they use UV kNoTexture and the shader outputs white, so hundreds of elements fit in one
    // or a few draw calls. Renderer2D adds the GPU layer.
    class Batch2D {
    public:
        // UV for untextured quads (ui.frag uses white)
        static constexpr float kNoTexture = -1.0f;

        void clear();

        void rect(const glm::vec2& pos, const glm::vec2& size, const glm::vec4& color);
        // Inward border of the rectangle
        void rectOutline(const glm::vec2& pos, const glm::vec2& size, const glm::vec4& color,
                         float thickness = 1.0f);
        void line(const glm::vec2& a, const glm::vec2& b, const glm::vec4& color, float thickness = 1.0f);
        // uvMin/uvMax in Texture2D convention (v pointing up: (0, 0) = bottom-left of the image)
        void image(const Texture2D* texture, const glm::vec2& pos, const glm::vec2& size,
                   const glm::vec2& uvMin = glm::vec2(0.0f), const glm::vec2& uvMax = glm::vec2(1.0f),
                   const glm::vec4& tint = glm::vec4(1.0f), bool nearest = false);
        // Text using font (its atlas already uploaded to atlasTexture). pos = top-left of the first line;
        // '\n' moves down a line. Returns the occupied size (like Font::measure)
        glm::vec2 text(const Font& font, const Texture2D* atlasTexture, const glm::vec2& pos,
                       std::string_view text, const glm::vec4& color, float scale = 1.0f);

        // Clipping: anything outside is not drawn. Nested (intersection)
        void pushClip(const Rect2D& rect);
        void popClip();

        const std::vector<GpuVertex>& vertices() const { return m_vertices; }
        const std::vector<Draw2DCommand>& commands() const { return m_commands; }
        std::size_t quadCount() const { return m_vertices.size() / 6; }

    private:
        // Axis-aligned quad (p0 top-left) and quad with 4 free corners. textured = false: untextured
        // (does not break the batch)
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
