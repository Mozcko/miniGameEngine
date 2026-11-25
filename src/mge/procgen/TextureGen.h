#pragma once

#include <mge/procgen/Image.h>

#include <cstdint>
#include <initializer_list>
#include <vector>

#include <glm/glm.hpp>

namespace mge::texgen {

    // Region of an image (an atlas tile, or the entire image)
    struct Rect {
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
    };

    // Color gradient: sample(0) = first color, sample(1) = last
    class Palette {
    public:
        struct Stop {
            float t;
            glm::vec4 color;
        };
        Palette(std::initializer_list<Stop> stops);
        Palette(const glm::vec4& from, const glm::vec4& to) : Palette({{0.0f, from}, {1.0f, to}}) {}
        glm::vec4 sample(float t) const;

    private:
        std::vector<Stop> m_stops;
    };

    // All deterministic: same seed → same pixels. Only write inside rect.
    // Value-noise fbm (`scale` = cells per side of rect) mapped through the palette
    void noise(Image& image, const Rect& rect, const Palette& palette, float scale, std::uint32_t seed,
               int octaves = 3);
    // Scattered dots of another color over existing pixels (density 0..1)
    void speckle(Image& image, const Rect& rect, const glm::vec4& color, float density, std::uint32_t seed);
    // Offset brick rows with a `mortar`-pixel joint
    void bricks(Image& image, const Rect& rect, const Palette& brick, const glm::vec4& mortarColor, int rows,
                int columns, int mortar, std::uint32_t seed);
    // Horizontal wooden planks with grain and seams
    void planks(Image& image, const Rect& rect, const Palette& wood, int boards, std::uint32_t seed);
    // Mineral spots (ore veins) of `color` over existing pixels
    void ore(Image& image, const Rect& rect, const glm::vec4& color, int spots, int spotRadius,
             std::uint32_t seed);
    // Irregular vertical bands (bark)
    void stripes(Image& image, const Rect& rect, const Palette& palette, int count, std::uint32_t seed);
    // Concentric rings (cut log)
    void rings(Image& image, const Rect& rect, const Palette& palette, float spacing, std::uint32_t seed);
    // Cross plant: stem + flower or leaves on a transparent background (for the cutout pass)
    void plant(Image& image, const Rect& rect, const glm::vec4& stem, const glm::vec4& bloom, int bloomRadius,
               std::uint32_t seed);
    // Leaves: green noise with transparent holes (holes 0..1)
    void leaves(Image& image, const Rect& rect, const Palette& palette, float holes, std::uint32_t seed);
    // Border of `width` pixels (glass, frames)
    void frame(Image& image, const Rect& rect, const glm::vec4& color, int width);

    // Rect of tile `index` in an atlas with `columns` columns and `tileSize`-px tiles (tile 0 top-left)
    inline Rect tileRect(int index, int columns, int tileSize) {
        return {(index % columns) * tileSize, (index / columns) * tileSize, tileSize, tileSize};
    }
}
