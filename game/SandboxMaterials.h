#pragma once

#include <mge/procgen/Image.h>

#include <cstdint>
#include <vector>

namespace sandbox {

    // Sandbox atlas generated in code (#34): same tile positions as
    // Minecraft-like_3d_tileset.png (16×16 tiles of 32 px), including the animated water and lava
    // frames. With it, the sandbox renders without loading any image (ADR 0004: both paths).
    mge::Image buildProceduralAtlas(std::uint32_t seed = 1337);

    // Tiles painted by buildProceduralAtlas (those used by blocks + animation frames)
    const std::vector<int>& proceduralTiles();
}
