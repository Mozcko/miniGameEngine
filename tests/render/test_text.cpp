// Fuentes TTF opcionales (#41): no necesitan GPU, pero viven en mge_graphics (stb_truetype).
#include <doctest/doctest.h>
#include <mge/core/Log.h>
#include <mge/graphics/TtfFont.h>

#include <cstdlib>
#include <filesystem>
#include <vector>

using namespace mge;

namespace {
    // Una fuente del sistema para probar (el motor no trae ninguna). MGE_TEST_TTF la fija a mano
    std::filesystem::path findSystemTtf() {
        if (const char* env = std::getenv("MGE_TEST_TTF")) return env;
        for (const char* candidate : {"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
                                      "/usr/share/fonts/dejavu-sans-fonts/DejaVuSans.ttf",
                                      "/usr/share/fonts/google-droid-sans-fonts/DroidSans.ttf",
                                      "/usr/share/fonts/liberation-sans/LiberationSans-Regular.ttf"})
            if (std::filesystem::exists(candidate)) return candidate;
        return {};
    }

    struct QuietLog {
        QuietLog() {
            Log::setSink([](const LogRecord&) {});
        }
        ~QuietLog() { Log::setSink(nullptr); }
    };
}

TEST_CASE("Una TTF del sistema se hornea en un atlas con medidas coherentes") {
    const std::filesystem::path path = findSystemTtf();
    if (path.empty()) {
        MESSAGE("No hay una fuente TTF conocida en el sistema: se saltea (MGE_TEST_TTF=ruta para fijarla)");
        return;
    }
    const Font font = loadTtfFontFile(path, 24.0f);
    REQUIRE(font.valid());
    CHECK_FALSE(font.isPixelFont());
    CHECK(font.lineHeight() >= 24.0f);
    CHECK(font.lineHeight() <= 40.0f);

    const Glyph& m = font.glyph(U'M');
    const Glyph& dot = font.glyph(U'.');
    const Glyph& space = font.glyph(U' ');
    CHECK(m.size.x > 0);
    CHECK(m.size.y > 10);
    CHECK(space.size.x == 0); // stb no reserva píxeles para el espacio
    CHECK(space.advance > 0.0f);
    CHECK(m.advance > dot.advance); // Proporcional, no monoespaciada
    // La 'M' cuelga de la línea base: su parte de arriba queda por debajo del borde superior de la línea
    CHECK(m.offset.y >= 0.0f);
    CHECK(m.offset.y + float(m.size.y) <= font.lineHeight() + 1.0f);
    // La 'g' baja de la línea base más que la 'M'
    CHECK(font.glyph(U'g').offset.y + float(font.glyph(U'g').size.y) > m.offset.y + float(m.size.y));

    // Hay cobertura en el atlas donde está la 'M'
    float alpha = 0.0f;
    for (int y = 0; y < m.size.y; y++)
        for (int x = 0; x < m.size.x; x++) alpha += font.atlas().get(m.atlasPos.x + x, m.atlasPos.y + y).a;
    CHECK(alpha > 10.0f);

    CHECK(font.measure("MM").x == doctest::Approx(2.0f * m.advance));
}

TEST_CASE("Una TTF inválida o inexistente da una fuente inválida (y un error en el log)") {
    QuietLog quiet;
    CHECK_FALSE(loadTtfFont({}, 16.0f).valid());
    CHECK_FALSE(loadTtfFont(std::vector<std::uint8_t>(64, 0xAB), 16.0f).valid());
    CHECK_FALSE(loadTtfFontFile("/no/existe/fuente.ttf", 16.0f).valid());
}
