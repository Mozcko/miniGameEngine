#pragma once

#include <examples/platformer2d/Platformer.h>
#include <mge/graphics/Renderer2D.h>

#include <string>

#include <glm/glm.hpp>

namespace platformer {

    constexpr float kTilePixels = 32.0f; // Píxeles lógicos por casilla

    // Colores de un bioma (#BIOME del archivo)
    struct Palette {
        glm::vec4 sky, ground, dirt, bridge;
    };
    Palette paletteFor(const std::string& biome);

    // Escala entera para que el nivel (más un margen) entre a lo alto: píxeles nítidos
    float viewScale(const Level& level, std::uint32_t framebufferHeight);

    // Dibuja el nivel, los objetos, los enemigos, el jugador y el HUD en ui (entre begin y flush), con rects
    // de colores y la fuente embebida: no usa texturas. La cámara sigue al jugador en x
    void drawScene(mge::Renderer2D& ui, const Platformer& game, int levelNumber);
}
