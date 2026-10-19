#include <mge/procgen/Image.h>

#include <algorithm>
#include <array>
#include <fstream>

namespace mge {

    Image::Image(std::uint32_t width, std::uint32_t height, const glm::vec4& fill)
        : m_width(width), m_height(height), m_pixels(std::size_t(width) * height * 4) {
        this->fill(fill);
    }

    glm::vec4 Image::get(int x, int y) const {
        if (x < 0 || y < 0 || x >= int(m_width) || y >= int(m_height)) return glm::vec4(0.0f);
        const std::uint8_t* p = &m_pixels[(std::size_t(y) * m_width + std::size_t(x)) * 4];
        return glm::vec4(p[0], p[1], p[2], p[3]) / 255.0f;
    }

    void Image::set(int x, int y, const glm::vec4& color) {
        if (x < 0 || y < 0 || x >= int(m_width) || y >= int(m_height)) return;
        std::uint8_t* p = &m_pixels[(std::size_t(y) * m_width + std::size_t(x)) * 4];
        for (int c = 0; c < 4; c++)
            p[c] = static_cast<std::uint8_t>(std::clamp(color[c], 0.0f, 1.0f) * 255.0f + 0.5f);
    }

    void Image::fill(const glm::vec4& color) {
        for (std::uint32_t y = 0; y < m_height; y++)
            for (std::uint32_t x = 0; x < m_width; x++) set(int(x), int(y), color);
    }

    namespace {
        std::uint32_t crc32(const std::uint8_t* data, std::size_t size, std::uint32_t crc = 0xFFFFFFFFu) {
            static const std::array<std::uint32_t, 256> kTable = [] {
                std::array<std::uint32_t, 256> t{};
                for (std::uint32_t n = 0; n < 256; n++) {
                    std::uint32_t c = n;
                    for (int k = 0; k < 8; k++) c = (c & 1u) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
                    t[n] = c;
                }
                return t;
            }();
            for (std::size_t i = 0; i < size; i++) crc = kTable[(crc ^ data[i]) & 0xFFu] ^ (crc >> 8);
            return crc;
        }

        void putU32(std::vector<std::uint8_t>& out, std::uint32_t v) {
            for (int shift = 24; shift >= 0; shift -= 8) out.push_back(static_cast<std::uint8_t>(v >> shift));
        }

        void chunk(std::vector<std::uint8_t>& out, const char type[4],
                   const std::vector<std::uint8_t>& payload) {
            putU32(out, static_cast<std::uint32_t>(payload.size()));
            const std::size_t start = out.size();
            out.insert(out.end(), type, type + 4);
            out.insert(out.end(), payload.begin(), payload.end());
            putU32(out, crc32(&out[start], out.size() - start) ^ 0xFFFFFFFFu);
        }
    }

    bool writePng(const Image& image, const std::filesystem::path& path) {
        if (image.empty()) return false;
        // Datos crudos: cada fila empieza con el filtro 0 (ninguno)
        std::vector<std::uint8_t> raw;
        const std::size_t row = std::size_t(image.width()) * 4;
        raw.reserve((row + 1) * image.height());
        for (std::uint32_t y = 0; y < image.height(); y++) {
            raw.push_back(0);
            raw.insert(raw.end(), image.data() + y * row, image.data() + (y + 1) * row);
        }
        // zlib con bloques deflate "stored" (sin compresión) de hasta 65535 bytes, más Adler-32
        std::vector<std::uint8_t> zlib = {0x78, 0x01};
        for (std::size_t offset = 0; offset < raw.size() || offset == 0;) {
            const std::size_t length = std::min<std::size_t>(65535, raw.size() - offset);
            const bool last = offset + length >= raw.size();
            zlib.push_back(last ? 1 : 0);
            zlib.push_back(static_cast<std::uint8_t>(length & 0xFF));
            zlib.push_back(static_cast<std::uint8_t>(length >> 8));
            zlib.push_back(static_cast<std::uint8_t>(~length & 0xFF));
            zlib.push_back(static_cast<std::uint8_t>((~length >> 8) & 0xFF));
            zlib.insert(zlib.end(), raw.begin() + std::ptrdiff_t(offset),
                        raw.begin() + std::ptrdiff_t(offset + length));
            offset += length;
            if (last) break;
        }
        std::uint32_t a = 1, b = 0;
        for (std::uint8_t byte : raw) {
            a = (a + byte) % 65521;
            b = (b + a) % 65521;
        }
        putU32(zlib, (b << 16) | a);

        std::vector<std::uint8_t> png = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
        std::vector<std::uint8_t> header;
        putU32(header, image.width());
        putU32(header, image.height());
        header.insert(header.end(),
                      {8, 6, 0, 0, 0}); // 8 bits, RGBA, deflate, filtro adaptativo, sin entrelazado
        chunk(png, "IHDR", header);
        chunk(png, "IDAT", zlib);
        chunk(png, "IEND", {});

        std::ofstream file(path, std::ios::binary);
        if (!file) return false;
        file.write(reinterpret_cast<const char*>(png.data()), std::streamsize(png.size()));
        return static_cast<bool>(file);
    }
}
