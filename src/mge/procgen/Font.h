#pragma once

#include <mge/procgen/Image.h>

#include <cstddef>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>

namespace mge {

    // Un carácter dentro del atlas de una fuente. Todo en píxeles de la fuente (escala 1)
    struct Glyph {
        glm::ivec2 atlasPos{0}; // Esquina superior izquierda en el atlas (fila 0 arriba, como Image)
        glm::ivec2 size{0};     // 0 = nada que dibujar (espacio)
        glm::vec2 offset{0.0f}; // Desde la posición del lápiz (arriba a la izquierda de la línea) al glifo
        float advance = 0.0f;   // Cuánto avanza el lápiz
    };

    // Fuente lista para dibujar texto (#41): un atlas RGBA (blanco + alfa) y las medidas de cada carácter
    // ASCII 32..126. Headless: la arma la CPU, el renderer 2D la sube a la GPU.
    //   Font::builtin()        → fuente 8x8 embebida en el código (sin archivos, nítida a escala entera)
    //   loadTtfFont(...)       → TTF con stb_truetype (mge_graphics, opcional)
    // El texto se lee como UTF-8; lo que no está en la fuente se dibuja como '?'.
    class Font {
    public:
        static constexpr char32_t kFirstChar = 32;
        static constexpr char32_t kLastChar = 126;
        static constexpr std::size_t kGlyphCount = kLastChar - kFirstChar + 1;

        Font() = default;
        // glyphs: uno por carácter de kFirstChar a kLastChar. pixelFont: muestrear sin filtrar
        Font(Image atlas, std::vector<Glyph> glyphs, float lineHeight, bool pixelFont);

        // La fuente 8x8 embebida: atlas de 16x6 glifos (128x48 px), línea de 10 px
        static Font builtin();

        bool valid() const { return !m_atlas.empty() && m_glyphs.size() == kGlyphCount; }
        const Image& atlas() const { return m_atlas; }
        bool isPixelFont() const { return m_pixelFont; }
        float lineHeight() const { return m_lineHeight; }

        // Glifo de un carácter; si no existe, el de '?'
        const Glyph& glyph(char32_t codepoint) const;
        // Tamaño del texto en píxeles (ancho de la línea más larga × líneas), con la escala dada
        glm::vec2 measure(std::string_view text, float scale = 1.0f) const;

    private:
        Image m_atlas;
        std::vector<Glyph> m_glyphs;
        float m_lineHeight = 0.0f;
        bool m_pixelFont = false;
    };

    // Lee un carácter UTF-8 de text a partir de index y avanza index. Secuencias inválidas → U+FFFD
    char32_t nextCodepoint(std::string_view text, std::size_t& index);

    // Bits de un glifo de la fuente 8x8 (8 filas de arriba a abajo, bit 0 = píxel izquierdo); nullptr fuera
    // de ASCII 32..126
    const unsigned char* builtinFontGlyphBits(char32_t codepoint);
}
