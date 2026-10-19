#pragma once

#include <mge/procgen/Image.h>

#include <cstdint>
#include <vector>

namespace sandbox {

    // Atlas del sandbox generado por código (#34): mismas posiciones de tiles que
    // Minecraft-like_3d_tileset.png (16×16 tiles de 32 px), incluidos los cuadros animados del agua y la
    // lava. Con él, el sandbox se dibuja sin cargar ninguna imagen (ADR 0004: los dos caminos).
    mge::Image buildProceduralAtlas(std::uint32_t seed = 1337);

    // Tiles que pinta buildProceduralAtlas (los que usan los bloques + los cuadros de animación)
    const std::vector<int>& proceduralTiles();
}
