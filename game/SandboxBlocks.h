#pragma once

#include <mge/voxel/Block.h>

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
    inline constexpr int kWaterTile = 207;
    inline constexpr int kLavaTile = 239;
}
