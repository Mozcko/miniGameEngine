#pragma once

#include <mge/voxel/Block.h>

#include <array>

namespace sandbox {

    // Sandbox block IDs within its BlockRegistry
    struct SandboxBlocks {
        mge::BlockId grass, dirt, stone, planks, refinedStone, brick, tnt, cobweb, rose, flower, sapling,
            cobblestone, coal, sand, gravel, log, glass, leaves, water, lava;
    };

    // Registers the sandbox blocks. Each has atlas tiles (Minecraft-like_3d_tileset.png,
    // 16×16 tiles of 32 px) and also a flat color, for drawing without textures (procedural path).
    SandboxBlocks registerSandboxBlocks(mge::BlockRegistry& registry);

    // Sandbox atlas and animated tiles (water and lava are overwritten every 0.2 s)
    inline constexpr int kAtlasColumns = 16;
    inline constexpr int kAtlasRows = 16;
    inline constexpr int kAtlasTileSize = 32;
    inline constexpr int kWaterTile = 207;
    inline constexpr int kLavaTile = 239;
    // Animation frames (copied over kWaterTile / kLavaTile)
    inline constexpr std::array<int, 5> kWaterFrames = {205, 206, 207, 222, 223};
    inline constexpr std::array<int, 5> kLavaFrames = {237, 238, 239, 254, 255};
}
