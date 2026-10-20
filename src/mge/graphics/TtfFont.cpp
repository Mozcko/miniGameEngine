#include <mge/graphics/TtfFont.h>

#include <mge/core/Assets.h>
#include <mge/core/Log.h>

#include <cmath>
#include <fstream>
#include <iterator>

// stb_truetype con funciones static (como stb_image en Texture2D.cpp)
#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb/stb_truetype.h>

namespace mge {

    namespace {
        constexpr int kMaxAtlasSize = 2048;
    }

    Font loadTtfFont(const std::vector<std::uint8_t>& ttfBytes, float pixelHeight) {
        if (ttfBytes.empty() || !(pixelHeight > 0.0f)) {
            MGE_LOG_ERROR("Font", "TTF vacío o tamaño inválido (", pixelHeight, " px)");
            return {};
        }
        stbtt_fontinfo info;
        const int offset = stbtt_GetFontOffsetForIndex(ttfBytes.data(), 0);
        if (offset < 0 || !stbtt_InitFont(&info, ttfBytes.data(), offset)) {
            MGE_LOG_ERROR("Font", "El archivo no es una fuente TrueType válida");
            return {};
        }
        int ascent = 0, descent = 0, lineGap = 0;
        stbtt_GetFontVMetrics(&info, &ascent, &descent, &lineGap);
        const float scale = stbtt_ScaleForPixelHeight(&info, pixelHeight);

        // Atlas cada vez más grande hasta que entren los 95 glifos
        stbtt_bakedchar baked[Font::kGlyphCount];
        std::vector<unsigned char> coverage;
        int size = 128;
        for (;; size *= 2) {
            if (size > kMaxAtlasSize) {
                MGE_LOG_ERROR("Font", "Los glifos de ", pixelHeight, " px no entran en un atlas de ",
                              kMaxAtlasSize, " px");
                return {};
            }
            coverage.assign(std::size_t(size) * std::size_t(size), 0);
            if (stbtt_BakeFontBitmap(ttfBytes.data(), offset, pixelHeight, coverage.data(), size, size,
                                     int(Font::kFirstChar), int(Font::kGlyphCount), baked) > 0)
                break;
        }

        // Blanco con la cobertura en el alfa: el color lo pone quien dibuja
        Image atlas{std::uint32_t(size), std::uint32_t(size)};
        for (int y = 0; y < size; y++)
            for (int x = 0; x < size; x++) {
                const unsigned char a = coverage[std::size_t(y) * std::size_t(size) + std::size_t(x)];
                if (a) atlas.set(x, y, glm::vec4(1.0f, 1.0f, 1.0f, float(a) / 255.0f));
            }

        const float baseline = std::round(float(ascent) * scale);
        std::vector<Glyph> glyphs(Font::kGlyphCount);
        for (std::size_t i = 0; i < Font::kGlyphCount; i++) {
            const stbtt_bakedchar& b = baked[i];
            Glyph& g = glyphs[i];
            g.atlasPos = glm::ivec2(b.x0, b.y0);
            g.size = glm::ivec2(b.x1 - b.x0, b.y1 - b.y0);
            g.offset = glm::vec2(b.xoff, baseline + b.yoff); // yoff es desde la línea base
            g.advance = b.xadvance;
        }
        const float lineHeight = std::ceil(float(ascent - descent + lineGap) * scale);
        return Font(std::move(atlas), std::move(glyphs), lineHeight, /*pixelFont=*/false);
    }

    Font loadTtfFontFile(const std::filesystem::path& path, float pixelHeight) {
        const std::filesystem::path full = path.is_absolute() ? path : assetPath(path.generic_string());
        std::ifstream file(full, std::ios::binary);
        if (!file) {
            MGE_LOG_ERROR("Font", "No se pudo abrir ", full.string());
            return {};
        }
        const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)),
                                              std::istreambuf_iterator<char>());
        Font font = loadTtfFont(bytes, pixelHeight);
        if (font.valid())
            MGE_LOG_INFO("Font", "Fuente ", full.filename().string(), " (", pixelHeight, " px)");
        return font;
    }
}
