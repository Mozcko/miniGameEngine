#pragma once

#include <mge/procgen/Image.h>

#include <cstdint>
#include <initializer_list>
#include <vector>

#include <glm/glm.hpp>

namespace mge::texgen {

    // Región de una imagen (un tile de un atlas, o la imagen entera)
    struct Rect {
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
    };

    // Degradado de colores: sample(0) = primer color, sample(1) = último
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

    // Todos deterministas: misma semilla → mismos píxeles. Escriben solo dentro de rect.
    // Ruido fbm (value noise, `scale` = celdas por lado del rect) pasado por la paleta
    void noise(Image& image, const Rect& rect, const Palette& palette, float scale, std::uint32_t seed,
               int octaves = 3);
    // Puntos sueltos de otro color sobre lo que ya hay (densidad 0..1)
    void speckle(Image& image, const Rect& rect, const glm::vec4& color, float density, std::uint32_t seed);
    // Ladrillos desfasados por fila con junta de `mortar` píxeles
    void bricks(Image& image, const Rect& rect, const Palette& brick, const glm::vec4& mortarColor, int rows,
                int columns, int mortar, std::uint32_t seed);
    // Tablas horizontales con vetas y separaciones
    void planks(Image& image, const Rect& rect, const Palette& wood, int boards, std::uint32_t seed);
    // Manchas (vetas de mineral) de `color` sobre lo que ya hay
    void ore(Image& image, const Rect& rect, const glm::vec4& color, int spots, int spotRadius,
             std::uint32_t seed);
    // Franjas verticales irregulares (corteza)
    void stripes(Image& image, const Rect& rect, const Palette& palette, int count, std::uint32_t seed);
    // Anillos concéntricos (tronco cortado)
    void rings(Image& image, const Rect& rect, const Palette& palette, float spacing, std::uint32_t seed);
    // Planta en cruz: tallo + flor o hojas sobre fondo transparente (para la pasada cutout)
    void plant(Image& image, const Rect& rect, const glm::vec4& stem, const glm::vec4& bloom, int bloomRadius,
               std::uint32_t seed);
    // Hojas: ruido verde con huecos transparentes (holes 0..1)
    void leaves(Image& image, const Rect& rect, const Palette& palette, float holes, std::uint32_t seed);
    // Borde de `width` píxeles (vidrio, marcos)
    void frame(Image& image, const Rect& rect, const glm::vec4& color, int width);

    // Rect del tile `index` en un atlas de columns columnas y tiles de tileSize px (tile 0 arriba a la
    // izquierda)
    inline Rect tileRect(int index, int columns, int tileSize) {
        return {(index % columns) * tileSize, (index / columns) * tileSize, tileSize, tileSize};
    }
}
