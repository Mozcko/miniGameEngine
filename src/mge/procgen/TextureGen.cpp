#include <mge/procgen/TextureGen.h>

#include <mge/core/Random.h>
#include <mge/procgen/Noise.h>

#include <algorithm>
#include <cmath>

namespace mge::texgen {

    Palette::Palette(std::initializer_list<Stop> stops) : m_stops(stops) {
        std::sort(m_stops.begin(), m_stops.end(), [](const Stop& a, const Stop& b) { return a.t < b.t; });
    }

    glm::vec4 Palette::sample(float t) const {
        if (m_stops.empty()) return glm::vec4(1.0f);
        if (t <= m_stops.front().t) return m_stops.front().color;
        for (std::size_t i = 1; i < m_stops.size(); i++) {
            if (t <= m_stops[i].t) {
                const Stop& a = m_stops[i - 1];
                const Stop& b = m_stops[i];
                return glm::mix(a.color, b.color, (t - a.t) / std::max(b.t - a.t, 1e-6f));
            }
        }
        return m_stops.back().color;
    }

    namespace {
        // Recorre los píxeles del rect (coordenadas locales u, v en píxeles)
        template <typename F> void each(const Rect& rect, F&& fn) {
            for (int v = 0; v < rect.height; v++)
                for (int u = 0; u < rect.width; u++) fn(u, v, rect.x + u, rect.y + v);
        }

        float unit(std::uint32_t h) {
            return float(h & 0xFFFFFFu) / float(0xFFFFFFu);
        }
    }

    void noise(Image& image, const Rect& rect, const Palette& palette, float scale, std::uint32_t seed,
               int octaves) {
        const Noise n(seed);
        FractalSettings settings;
        settings.octaves = octaves;
        settings.frequency = scale / float(std::max(rect.width, 1));
        each(rect, [&](int u, int v, int x, int y) {
            const float value = n.fbm(NoiseType::Value, float(u), float(v), settings) * 0.5f + 0.5f;
            image.set(x, y, palette.sample(value));
        });
    }

    void speckle(Image& image, const Rect& rect, const glm::vec4& color, float density, std::uint32_t seed) {
        each(rect, [&](int u, int v, int x, int y) {
            if (unit(hash2i(u, v, seed)) < density) image.set(x, y, color);
        });
    }

    void bricks(Image& image, const Rect& rect, const Palette& brick, const glm::vec4& mortarColor, int rows,
                int columns, int mortar, std::uint32_t seed) {
        const int brickH = std::max(1, rect.height / rows);
        const int brickW = std::max(1, rect.width / columns);
        each(rect, [&](int u, int v, int x, int y) {
            const int row = v / brickH;
            const int shifted = u + (row % 2) * brickW / 2; // Filas desfasadas medio ladrillo
            const int col = shifted / brickW;
            const bool joint = (v % brickH) < mortar || (shifted % brickW) < mortar;
            if (joint) {
                image.set(x, y, mortarColor);
                return;
            }
            // Cada ladrillo con su tono, y un poco de grano
            const float tone = unit(hash2i(col, row, seed));
            const float grain = unit(hash2i(u, v, seed + 1)) * 0.15f;
            image.set(x, y, brick.sample(std::clamp(tone * 0.85f + grain, 0.0f, 1.0f)));
        });
    }

    void planks(Image& image, const Rect& rect, const Palette& wood, int boards, std::uint32_t seed) {
        const int boardH = std::max(1, rect.height / boards);
        const Noise n(seed);
        each(rect, [&](int u, int v, int x, int y) {
            const int board = v / boardH;
            if (v % boardH == boardH - 1) {
                image.set(x, y, wood.sample(0.0f) * glm::vec4(0.6f, 0.6f, 0.6f, 1.0f)); // Separación oscura
                return;
            }
            // Vetas a lo largo de la tabla
            const float grain =
                n.value(float(u) * 0.08f, float(v) * 0.9f + float(board) * 7.0f) * 0.5f + 0.5f;
            const float tone = unit(hash2i(board, 0, seed)) * 0.3f;
            image.set(x, y, wood.sample(std::clamp(grain * 0.7f + tone, 0.0f, 1.0f)));
        });
    }

    void ore(Image& image, const Rect& rect, const glm::vec4& color, int spots, int spotRadius,
             std::uint32_t seed) {
        Pcg32 rng(seed, 3);
        for (int s = 0; s < spots; s++) {
            const int cx = rng.range(spotRadius, std::max(spotRadius + 1, rect.width - spotRadius));
            const int cy = rng.range(spotRadius, std::max(spotRadius + 1, rect.height - spotRadius));
            each(rect, [&](int u, int v, int x, int y) {
                const int dx = u - cx, dy = v - cy;
                const float jitter = unit(hash2i(u, v, seed + std::uint32_t(s))) * 1.5f;
                if (float(dx * dx + dy * dy) <= float(spotRadius * spotRadius) + jitter)
                    image.set(x, y, color);
            });
        }
    }

    void stripes(Image& image, const Rect& rect, const Palette& palette, int count, std::uint32_t seed) {
        const Noise n(seed);
        each(rect, [&](int u, int v, int x, int y) {
            const float wobble = n.value(float(v) * 0.15f, 0.0f) * 2.0f;
            const float band = std::sin((float(u) + wobble) / float(rect.width) * float(count) * 6.2831853f);
            const float grain = unit(hash2i(u, v, seed)) * 0.2f;
            image.set(x, y, palette.sample(std::clamp(band * 0.4f + 0.5f + grain - 0.1f, 0.0f, 1.0f)));
        });
    }

    void rings(Image& image, const Rect& rect, const Palette& palette, float spacing, std::uint32_t seed) {
        const float cx = float(rect.width) * 0.5f, cy = float(rect.height) * 0.5f;
        each(rect, [&](int u, int v, int x, int y) {
            const float d = std::hypot(float(u) + 0.5f - cx, float(v) + 0.5f - cy);
            const float ring = std::sin(d / spacing * 6.2831853f) * 0.5f + 0.5f;
            const float grain = unit(hash2i(u, v, seed)) * 0.15f;
            image.set(x, y, palette.sample(std::clamp(ring * 0.8f + grain, 0.0f, 1.0f)));
        });
    }

    void plant(Image& image, const Rect& rect, const glm::vec4& stem, const glm::vec4& bloom, int bloomRadius,
               std::uint32_t seed) {
        each(rect, [&](int, int, int x, int y) { image.set(x, y, glm::vec4(0.0f)); }); // Transparente
        const int cx = rect.width / 2 + int(unit(hash2i(0, 0, seed)) * 4.0f) - 2;
        const int bloomY = rect.height / 3;
        // Tallo con una hoja a cada lado
        for (int v = bloomY; v < rect.height; v++) {
            image.set(rect.x + cx, rect.y + v, stem);
            image.set(rect.x + cx + 1, rect.y + v, stem * glm::vec4(0.8f, 0.8f, 0.8f, 1.0f));
        }
        const int leafY = rect.height * 2 / 3;
        for (int i = 1; i <= rect.width / 6; i++) {
            image.set(rect.x + cx - i, rect.y + leafY - i / 2, stem);
            image.set(rect.x + cx + 1 + i, rect.y + leafY - i / 3, stem);
        }
        // Flor (o copita de hojas)
        each(rect, [&](int u, int v, int x, int y) {
            const int dx = u - cx, dy = v - bloomY;
            if (dx * dx + dy * dy <= bloomRadius * bloomRadius) {
                const float shade = 0.85f + unit(hash2i(u, v, seed)) * 0.15f;
                image.set(x, y, glm::vec4(glm::vec3(bloom) * shade, bloom.a));
            }
        });
    }

    void leaves(Image& image, const Rect& rect, const Palette& palette, float holes, std::uint32_t seed) {
        noise(image, rect, palette, 6.0f, seed, 2);
        each(rect, [&](int u, int v, int x, int y) {
            if (unit(hash2i(u / 2, v / 2, seed + 7)) < holes)
                image.set(x, y, glm::vec4(0.0f)); // Huecos de 2×2
        });
    }

    void frame(Image& image, const Rect& rect, const glm::vec4& color, int width) {
        each(rect, [&](int u, int v, int x, int y) {
            if (u < width || v < width || u >= rect.width - width || v >= rect.height - width)
                image.set(x, y, color);
        });
    }
}
