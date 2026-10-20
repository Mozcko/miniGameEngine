#include <doctest/doctest.h>
#include <mge/procgen/Font.h>

#include <string>

using namespace mge;

TEST_CASE("La fuente embebida arma un atlas 128x48 con un glifo por carácter ASCII imprimible") {
    const Font font = Font::builtin();
    REQUIRE(font.valid());
    CHECK(font.isPixelFont());
    CHECK(font.atlas().width() == 128);
    CHECK(font.atlas().height() == 48);
    CHECK(font.lineHeight() == doctest::Approx(10.0f));
    CHECK(font.glyph(U'A').advance == doctest::Approx(8.0f));
    CHECK(font.glyph(U' ').size == glm::ivec2(0)); // Nada que dibujar, pero avanza
    CHECK(font.glyph(U' ').advance == doctest::Approx(8.0f));
}

TEST_CASE("Los píxeles del atlas son los bits de la tabla (fila 0 arriba, bit 0 a la izquierda)") {
    const Font font = Font::builtin();
    for (char32_t c : {U'A', U'g', U'~', U'0', U'@'}) {
        CAPTURE(static_cast<int>(c));
        const Glyph& g = font.glyph(c);
        const unsigned char* bits = builtinFontGlyphBits(c);
        REQUIRE(bits != nullptr);
        for (int y = 0; y < 8; y++)
            for (int x = 0; x < 8; x++) {
                const bool on = (bits[y] >> x & 1u) != 0;
                const glm::vec4 px = font.atlas().get(g.atlasPos.x + x, g.atlasPos.y + y);
                CHECK(px.a == doctest::Approx(on ? 1.0f : 0.0f));
            }
    }
    // 'A' derecha: el vértice (fila de arriba) es angosto y la barra está más abajo
    const unsigned char* a = builtinFontGlyphBits(U'A');
    CHECK(a[0] == 0x0C);
    CHECK(a[4] == 0x3F);
}

TEST_CASE("Lo que no está en la fuente se dibuja como '?'") {
    const Font font = Font::builtin();
    const Glyph& question = font.glyph(U'?');
    CHECK(font.glyph(U'\t').atlasPos == question.atlasPos);
    CHECK(font.glyph(U'ñ').atlasPos == question.atlasPos);
    CHECK(font.glyph(0x1F600).atlasPos == question.atlasPos);
    CHECK(builtinFontGlyphBits(U'ñ') == nullptr);
}

TEST_CASE("measure: ancho de la línea más larga por la escala, alto por líneas") {
    const Font font = Font::builtin();
    CHECK(font.measure("") == glm::vec2(0.0f));
    CHECK(font.measure("FPS: 60") == glm::vec2(7 * 8, 10));
    CHECK(font.measure("FPS: 60", 2.0f) == glm::vec2(7 * 16, 20));
    CHECK(font.measure("ab\nabcd\nc") == glm::vec2(4 * 8, 30));
    CHECK(font.measure("año") == glm::vec2(3 * 8, 10)); // UTF-8: 'ñ' es un carácter ('?'), no dos
}

TEST_CASE("nextCodepoint decodifica UTF-8 y reemplaza secuencias inválidas") {
    const std::string text = "a\xC3\xB1\xE2\x82\xAC\xF0\x9F\x98\x80\xFF";
    std::size_t i = 0;
    CHECK(nextCodepoint(text, i) == U'a');
    CHECK(nextCodepoint(text, i) == 0xF1);    // ñ
    CHECK(nextCodepoint(text, i) == 0x20AC);  // €
    CHECK(nextCodepoint(text, i) == 0x1F600); // 😀
    CHECK(nextCodepoint(text, i) == 0xFFFD);  // Byte inválido
    CHECK(i == text.size());

    const std::string cut = "\xE2\x82"; // Secuencia cortada
    i = 0;
    CHECK(nextCodepoint(cut, i) == 0xFFFD);
}

TEST_CASE("La fuente embebida es determinista") {
    CHECK(Font::builtin().atlas().pixels() == Font::builtin().atlas().pixels());
}

TEST_CASE("Una fuente vacía no es válida y no se rompe") {
    const Font font;
    CHECK_FALSE(font.valid());
    CHECK(font.glyph(U'A').advance == doctest::Approx(0.0f));
    CHECK(font.measure("abc") == glm::vec2(0.0f));
}
