#include <doctest/doctest.h>
#include <mge/procgen/Font.h>

#include <string>

using namespace mge;

TEST_CASE("Built-in font builds a 128x48 atlas with one glyph per printable ASCII character") {
    const Font font = Font::builtin();
    REQUIRE(font.valid());
    CHECK(font.isPixelFont());
    CHECK(font.atlas().width() == 128);
    CHECK(font.atlas().height() == 48);
    CHECK(font.lineHeight() == doctest::Approx(10.0f));
    CHECK(font.glyph(U'A').advance == doctest::Approx(8.0f));
    CHECK(font.glyph(U' ').size == glm::ivec2(0)); // Nothing to draw, but it still advances
    CHECK(font.glyph(U' ').advance == doctest::Approx(8.0f));
}

TEST_CASE("Atlas pixels match the table bits (row 0 at top, bit 0 on the left)") {
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
    // Upright 'A': the apex (top row) is narrow and the crossbar is lower down
    const unsigned char* a = builtinFontGlyphBits(U'A');
    CHECK(a[0] == 0x0C);
    CHECK(a[4] == 0x3F);
}

TEST_CASE("Characters missing from the font are drawn as '?'") {
    const Font font = Font::builtin();
    const Glyph& question = font.glyph(U'?');
    CHECK(font.glyph(U'\t').atlasPos == question.atlasPos);
    CHECK(font.glyph(U'ñ').atlasPos == question.atlasPos);
    CHECK(font.glyph(0x1F600).atlasPos == question.atlasPos);
    CHECK(builtinFontGlyphBits(U'ñ') == nullptr);
}

TEST_CASE("measure: width of longest line times scale, height by line count") {
    const Font font = Font::builtin();
    CHECK(font.measure("") == glm::vec2(0.0f));
    CHECK(font.measure("FPS: 60") == glm::vec2(7 * 8, 10));
    CHECK(font.measure("FPS: 60", 2.0f) == glm::vec2(7 * 16, 20));
    CHECK(font.measure("ab\nabcd\nc") == glm::vec2(4 * 8, 30));
    CHECK(font.measure("año") == glm::vec2(3 * 8, 10)); // UTF-8: 'ñ' is one character ('?'), not two
}

TEST_CASE("nextCodepoint decodes UTF-8 and replaces invalid sequences") {
    const std::string text = "a\xC3\xB1\xE2\x82\xAC\xF0\x9F\x98\x80\xFF";
    std::size_t i = 0;
    CHECK(nextCodepoint(text, i) == U'a');
    CHECK(nextCodepoint(text, i) == 0xF1);    // ñ
    CHECK(nextCodepoint(text, i) == 0x20AC);  // €
    CHECK(nextCodepoint(text, i) == 0x1F600); // 😀
    CHECK(nextCodepoint(text, i) == 0xFFFD);  // Invalid byte
    CHECK(i == text.size());

    const std::string cut = "\xE2\x82"; // Truncated sequence
    i = 0;
    CHECK(nextCodepoint(cut, i) == 0xFFFD);
}

TEST_CASE("Built-in font is deterministic") {
    CHECK(Font::builtin().atlas().pixels() == Font::builtin().atlas().pixels());
}

TEST_CASE("An empty font is invalid and does not crash") {
    const Font font;
    CHECK_FALSE(font.valid());
    CHECK(font.glyph(U'A').advance == doctest::Approx(0.0f));
    CHECK(font.measure("abc") == glm::vec2(0.0f));
}
