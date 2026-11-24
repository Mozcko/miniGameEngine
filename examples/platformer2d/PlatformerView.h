#pragma once

#include <examples/platformer2d/Platformer.h>
#include <mge/graphics/Renderer2D.h>

#include <string>

#include <glm/glm.hpp>

namespace platformer {

    constexpr float kTilePixels = 32.0f; // Logical pixels per tile

    // Biome colors (#BIOME in the level file)
    struct Palette {
        glm::vec4 sky, ground, dirt, bridge;
    };
    Palette paletteFor(const std::string& biome);

    // Integer scale so the level (plus margin) fits vertically: crisp pixels
    float viewScale(const Level& level, std::uint32_t framebufferHeight);

    // Draws the level, items, enemies, player, and HUD into ui (between begin and flush), using colored
    // rects and the embedded font: no textures. The camera follows the player along x
    void drawScene(mge::Renderer2D& ui, const Platformer& game, int levelNumber);
}
