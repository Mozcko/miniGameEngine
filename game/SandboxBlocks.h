#pragma once

#include <mge/voxel/Block.h>

#include <array>

namespace sandbox {

    // Ids de los bloques del sandbox dentro de su BlockRegistry
    struct SandboxBlocks {
        mge::BlockId grass, dirt, stone, planks, refinedStone, brick, tnt, cobweb, rose, flower, sapling,
            cobblestone, coal, sand, gravel, log, glass, leaves, water, lava;
    };

    // Registra los bloques del sandbox. Cada uno tiene tiles del atlas (Minecraft-like_3d_tileset.png,
    // 16×16 tiles de 32 px) y también un color plano, para dibujarlo sin texturas (camino procedural).
    SandboxBlocks registerSandboxBlocks(mge::BlockRegistry& registry);

    // Atlas del sandbox y tiles animados (agua y lava se reescriben cada 0.2 s)
    inline constexpr int kAtlasColumns = 16;
    inline constexpr int kAtlasRows = 16;
    inline constexpr int kAtlasTileSize = 32;
    inline constexpr int kWaterTile = 207;
    inline constexpr int kLavaTile = 239;
    // Cuadros de la animación (se copian sobre kWaterTile / kLavaTile)
    inline constexpr std::array<int, 5> kWaterFrames = {205, 206, 207, 222, 223};
    inline constexpr std::array<int, 5> kLavaFrames = {237, 238, 239, 254, 255};
}
