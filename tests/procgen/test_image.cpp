#include <doctest/doctest.h>
#include <mge/procgen/Image.h>
#include <mge/procgen/TextureGen.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

using namespace mge;

namespace {
    std::uint32_t readU32(const std::vector<std::uint8_t>& d, std::size_t at) {
        return (std::uint32_t(d[at]) << 24) | (std::uint32_t(d[at + 1]) << 16) |
               (std::uint32_t(d[at + 2]) << 8) | d[at + 3];
    }

    std::uint32_t crc32(const std::uint8_t* data, std::size_t size) {
        std::uint32_t crc = 0xFFFFFFFFu;
        for (std::size_t i = 0; i < size; i++) {
            crc ^= data[i];
            for (int k = 0; k < 8; k++) crc = (crc & 1u) ? 0xEDB88320u ^ (crc >> 1) : crc >> 1;
        }
        return crc ^ 0xFFFFFFFFu;
    }

    // Lector mínimo para el PNG que escribe writePng: valida los CRC y desarma los bloques "stored"
    bool decodeOwnPng(const std::vector<std::uint8_t>& png, std::uint32_t& width, std::uint32_t& height,
                      std::vector<std::uint8_t>& rgba) {
        const std::uint8_t signature[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
        if (png.size() < 8 || !std::equal(signature, signature + 8, png.begin())) return false;
        std::vector<std::uint8_t> zlib;
        for (std::size_t at = 8; at + 12 <= png.size();) {
            const std::uint32_t length = readU32(png, at);
            const std::string type(png.begin() + std::ptrdiff_t(at + 4),
                                   png.begin() + std::ptrdiff_t(at + 8));
            if (crc32(&png[at + 4], length + 4) != readU32(png, at + 8 + length)) return false;
            if (type == "IHDR") {
                width = readU32(png, at + 8);
                height = readU32(png, at + 12);
            } else if (type == "IDAT") {
                zlib.insert(zlib.end(), png.begin() + std::ptrdiff_t(at + 8),
                            png.begin() + std::ptrdiff_t(at + 8 + length));
            }
            at += 12 + length;
        }
        std::vector<std::uint8_t> raw;
        for (std::size_t at = 2;;) { // Salta el encabezado zlib
            const bool last = zlib[at] & 1;
            const std::size_t length = zlib[at + 1] | (std::size_t(zlib[at + 2]) << 8);
            raw.insert(raw.end(), zlib.begin() + std::ptrdiff_t(at + 5),
                       zlib.begin() + std::ptrdiff_t(at + 5 + length));
            at += 5 + length;
            if (last) break;
        }
        const std::size_t row = std::size_t(width) * 4;
        if (raw.size() != (row + 1) * height) return false;
        rgba.clear();
        for (std::uint32_t y = 0; y < height; y++) {
            if (raw[y * (row + 1)] != 0) return false; // Filtro "ninguno"
            rgba.insert(rgba.end(), raw.begin() + std::ptrdiff_t(y * (row + 1) + 1),
                        raw.begin() + std::ptrdiff_t((y + 1) * (row + 1)));
        }
        return true;
    }
}

TEST_CASE("Image: set/get con recorte y fuera de rango") {
    Image image(4, 3, {0, 0, 1, 1});
    CHECK(image.width() == 4);
    CHECK(image.pixels().size() == 4 * 3 * 4);
    CHECK(image.get(0, 0) == glm::vec4(0, 0, 1, 1));
    image.set(1, 2, {2.0f, -1.0f, 0.5f, 1.0f}); // Se recorta a [0, 1]
    CHECK(image.get(1, 2).r == doctest::Approx(1.0f));
    CHECK(image.get(1, 2).g == doctest::Approx(0.0f));
    CHECK(image.get(1, 2).b == doctest::Approx(128.0f / 255.0f));
    CHECK(image.get(-1, 0) == glm::vec4(0.0f));
    image.set(9, 9, glm::vec4(1.0f)); // Se ignora
}

TEST_CASE("writePng: PNG válido (CRC, bloques stored) con los mismos píxeles, también > 64 KB") {
    Image image(150, 120);
    for (int y = 0; y < 120; y++)
        for (int x = 0; x < 150; x++)
            image.set(x, y, {x / 149.0f, y / 119.0f, 0.5f, (x + y) % 2 ? 1.0f : 0.5f});
    const auto path = std::filesystem::temp_directory_path() / "mge_test_image.png";
    REQUIRE(writePng(image, path));
    std::ifstream file(path, std::ios::binary);
    const std::vector<std::uint8_t> png((std::istreambuf_iterator<char>(file)),
                                        std::istreambuf_iterator<char>());
    std::uint32_t w = 0, h = 0;
    std::vector<std::uint8_t> rgba;
    REQUIRE(decodeOwnPng(png, w, h, rgba));
    CHECK(w == 150);
    CHECK(h == 120);
    CHECK(rgba == image.pixels()); // 150×120×4 + filas = 72 120 bytes: más de un bloque stored
    std::filesystem::remove(path);
    CHECK_FALSE(writePng(Image(), path)); // Vacía
}

TEST_CASE("texgen: deterministas por semilla y cada uno pinta solo su rect") {
    namespace tg = texgen;
    const tg::Palette palette({0.1f, 0.2f, 0.3f, 1.0f}, {0.8f, 0.7f, 0.6f, 1.0f});
    CHECK(palette.sample(0.0f) == glm::vec4(0.1f, 0.2f, 0.3f, 1.0f));
    CHECK(palette.sample(2.0f) == glm::vec4(0.8f, 0.7f, 0.6f, 1.0f));

    auto paintAll = [&](std::uint32_t seed) {
        Image image(64, 64, {1, 0, 1, 1});
        const tg::Rect r = tg::tileRect(1, 2, 32); // Tile de arriba a la derecha
        CHECK(r.x == 32);
        CHECK(r.y == 0);
        tg::noise(image, r, palette, 4.0f, seed);
        tg::speckle(image, r, {0, 0, 0, 1}, 0.1f, seed);
        tg::ore(image, r, {1, 1, 0, 1}, 3, 2, seed);
        tg::bricks(image, tg::tileRect(2, 2, 32), palette, {1, 1, 1, 1}, 4, 2, 1, seed);
        tg::planks(image, tg::tileRect(3, 2, 32), palette, 4, seed);
        return image;
    };
    const Image a = paintAll(5), b = paintAll(5), c = paintAll(6);
    CHECK(a.pixels() == b.pixels());
    CHECK(a.pixels() != c.pixels());
    CHECK(a.get(5, 5) == glm::vec4(1, 0, 1, 1)); // Tile 0 sin tocar
}

TEST_CASE("texgen: plantas y hojas tienen huecos transparentes (para la pasada cutout)") {
    namespace tg = texgen;
    Image image(32, 64);
    tg::plant(image, {0, 0, 32, 32}, {0.2f, 0.5f, 0.1f, 1}, {0.9f, 0.1f, 0.1f, 1}, 5, 1);
    tg::leaves(image, {0, 32, 32, 32}, tg::Palette({0, 0.3f, 0, 1}, {0.2f, 0.6f, 0.2f, 1}), 0.3f, 1);
    int transparentPlant = 0, opaquePlant = 0, transparentLeaves = 0;
    for (int y = 0; y < 32; y++)
        for (int x = 0; x < 32; x++) {
            (image.get(x, y).a < 0.5f ? transparentPlant : opaquePlant)++;
            transparentLeaves += image.get(x, y + 32).a < 0.5f;
        }
    CHECK(transparentPlant > 32 * 32 / 2); // Mayormente fondo
    CHECK(opaquePlant > 20);               // Tallo + flor
    CHECK(transparentLeaves > 32 * 32 / 10);
    CHECK(transparentLeaves < 32 * 32 / 2);
}
