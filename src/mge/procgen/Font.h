#pragma once

#include <mge/procgen/Image.h>

#include <cstddef>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>

namespace mge {

    // A single character inside a font atlas. All values are in font pixels (scale 1)
    struct Glyph {
        glm::ivec2 atlasPos{0}; // Top-left corner in the atlas (row 0 at top, like Image)
        glm::ivec2 size{0};     // 0 = nothing to draw (space)
        glm::vec2 offset{0.0f}; // From pen position (top-left of the line) to the glyph
        float advance = 0.0f;   // How far the pen advances
    };

    // Font ready for text rendering (#41): an RGBA atlas (white + alpha) and metrics for each
    // ASCII character 32..126. Headless: built on the CPU, uploaded to the GPU by the 2D renderer.
    //   Font::builtin()        → embedded 8x8 font (no files, crisp at integer scales)
    //   loadTtfFont(...)       → TTF via stb_truetype (mge_graphics, optional)
    // Text is decoded as UTF-8; characters not in the font are drawn as '?'.
    class Font {
    public:
        static constexpr char32_t kFirstChar = 32;
        static constexpr char32_t kLastChar = 126;
        static constexpr std::size_t kGlyphCount = kLastChar - kFirstChar + 1;

        Font() = default;
        // glyphs: one per character from kFirstChar to kLastChar. pixelFont: sample unfiltered
        Font(Image atlas, std::vector<Glyph> glyphs, float lineHeight, bool pixelFont);

        // Embedded 8x8 font: 16x6 glyph atlas (128x48 px), 10 px line height
        static Font builtin();

        bool valid() const { return !m_atlas.empty() && m_glyphs.size() == kGlyphCount; }
        const Image& atlas() const { return m_atlas; }
        bool isPixelFont() const { return m_pixelFont; }
        float lineHeight() const { return m_lineHeight; }

        // Glyph for a character; falls back to '?' if missing
        const Glyph& glyph(char32_t codepoint) const;
        // Text size in pixels (widest line × line count) at the given scale
        glm::vec2 measure(std::string_view text, float scale = 1.0f) const;

    private:
        Image m_atlas;
        std::vector<Glyph> m_glyphs;
        float m_lineHeight = 0.0f;
        bool m_pixelFont = false;
    };

    // Reads one UTF-8 character from text starting at index and advances index. Invalid sequences → U+FFFD
    char32_t nextCodepoint(std::string_view text, std::size_t& index);

    // Glyph bits for the 8x8 font (8 rows top-to-bottom, bit 0 = leftmost pixel); nullptr outside
    // ASCII 32..126
    const unsigned char* builtinFontGlyphBits(char32_t codepoint);
}
