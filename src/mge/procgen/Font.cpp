#include <mge/procgen/Font.h>

#include <algorithm>

namespace mge {

    namespace {
        constexpr int kBuiltinCell = 8;
        constexpr int kBuiltinColumns = 16;
        constexpr float kBuiltinLineHeight = 10.0f; // 8 px + 2 de interlineado
    }

    Font::Font(Image atlas, std::vector<Glyph> glyphs, float lineHeight, bool pixelFont)
        : m_atlas(std::move(atlas)), m_glyphs(std::move(glyphs)), m_lineHeight(lineHeight),
          m_pixelFont(pixelFont) {}

    Font Font::builtin() {
        const int rows = int((kGlyphCount + kBuiltinColumns - 1) / kBuiltinColumns);
        Image atlas(kBuiltinColumns * kBuiltinCell, std::uint32_t(rows * kBuiltinCell));
        std::vector<Glyph> glyphs(kGlyphCount);
        for (std::size_t i = 0; i < kGlyphCount; i++) {
            const glm::ivec2 cell(int(i) % kBuiltinColumns * kBuiltinCell,
                                  int(i) / kBuiltinColumns * kBuiltinCell);
            const unsigned char* bits = builtinFontGlyphBits(kFirstChar + char32_t(i));
            bool empty = true;
            for (int y = 0; y < kBuiltinCell; y++)
                for (int x = 0; x < kBuiltinCell; x++)
                    if (bits[y] >> x & 1u) {
                        atlas.set(cell.x + x, cell.y + y, glm::vec4(1.0f));
                        empty = false;
                    }
            Glyph& g = glyphs[i];
            g.atlasPos = cell;
            g.size = empty ? glm::ivec2(0) : glm::ivec2(kBuiltinCell);
            g.advance = float(kBuiltinCell);
        }
        return Font(std::move(atlas), std::move(glyphs), kBuiltinLineHeight, /*pixelFont=*/true);
    }

    const Glyph& Font::glyph(char32_t codepoint) const {
        static const Glyph kNone;
        if (m_glyphs.empty()) return kNone;
        if (codepoint < kFirstChar || codepoint > kLastChar) codepoint = U'?';
        return m_glyphs[codepoint - kFirstChar];
    }

    glm::vec2 Font::measure(std::string_view text, float scale) const {
        if (text.empty()) return glm::vec2(0.0f);
        float widest = 0.0f;
        float line = 0.0f;
        int lines = 1;
        for (std::size_t i = 0; i < text.size();) {
            const char32_t c = nextCodepoint(text, i);
            if (c == U'\n') {
                widest = std::max(widest, line);
                line = 0.0f;
                lines++;
                continue;
            }
            line += glyph(c).advance;
        }
        widest = std::max(widest, line);
        return glm::vec2(widest, float(lines) * m_lineHeight) * scale;
    }

    char32_t nextCodepoint(std::string_view text, std::size_t& index) {
        constexpr char32_t kReplacement = 0xFFFD;
        const auto byte = [&](std::size_t i) { return static_cast<unsigned char>(text[i]); };
        const unsigned char lead = byte(index++);
        if (lead < 0x80) return lead;
        int extra = 0;
        char32_t cp = 0;
        if ((lead & 0xE0) == 0xC0) {
            extra = 1;
            cp = lead & 0x1Fu;
        } else if ((lead & 0xF0) == 0xE0) {
            extra = 2;
            cp = lead & 0x0Fu;
        } else if ((lead & 0xF8) == 0xF0) {
            extra = 3;
            cp = lead & 0x07u;
        } else {
            return kReplacement; // Byte de continuación suelto o inválido
        }
        for (int k = 0; k < extra; k++) {
            if (index >= text.size() || (byte(index) & 0xC0) != 0x80) return kReplacement;
            cp = (cp << 6) | (byte(index++) & 0x3Fu);
        }
        return cp;
    }
}
