#pragma once

#include <game/SandboxBlocks.h>
#include <mge/core/Random.h>
#include <mge/procgen/Noise.h>
#include <mge/voxel/VoxelWorld.h>

#include <cstdint>

namespace sandbox {

    enum class Biome {
        Plains,    // Grass, trees, and flowers
        Desert,    // Sand (hot and dry)
        Mountains, // Rock and gravel, high elevation
        Shore,     // Sand next to water
    };

    struct TerrainSettings {
        std::uint32_t seed = 1337;
        bool caves = true; // 3D noise tunnels (never break the y = 0 floor or the surface)
    };

    // Noise-based sandbox terrain (#27): height via simplex fbm + mountains (ridged noise under a
    // mask), biomes by temperature and moisture, 3D noise caves, and decoration (trees, flowers).
    // Deterministic per (coord, seed) and stateless: each column is a pure function, and trees from
    // neighboring columns (even from another chunk) are reconstructed from their column. This way a tree
    // crosses chunk borders intact and the world does not depend on load order.
    class SandboxTerrain : public mge::TerrainGenerator {
    public:
        static constexpr std::uint32_t kDefaultSeed = 1337;
        static constexpr int kWorldChunksY = 4;                              // Height in chunks
        static constexpr int kWorldHeight = kWorldChunksY * mge::kChunkSize; // 64 blocks
        static constexpr int kWaterLevel = 20;
        static constexpr int kMaxSurfaceHeight = kWorldHeight - 8; // Leaves room for trees
        static constexpr int kTreeRadius = 2;                      // Canopy: up to 2 blocks from trunk

        explicit SandboxTerrain(const SandboxBlocks& blocks, TerrainSettings settings = {});

        void generate(const mge::ChunkCoord& coord, mge::ChunkData& chunk) const override;

        // Surface height at world column (x, z), in [1, kMaxSurfaceHeight]
        int surfaceHeight(int worldX, int worldZ) const { return column(worldX, worldZ).height; }
        Biome biome(int worldX, int worldZ) const { return column(worldX, worldZ).biome; }
        // Does a cave carve out this block? (only below the surface, never at y = 0)
        bool isCave(int worldX, int worldY, int worldZ, int surfaceHeight) const;

    private:
        // Output of the terrain pass for a column (pure function of (x, z, seed))
        struct Column {
            int height;
            Biome biome;
            mge::BlockId surface; // Block at y == height
            mge::BlockId filler;  // Layers beneath the surface (dirt, sand, stone)
        };
        Column column(int worldX, int worldZ) const;

        void placeTree(mge::ChunkData& chunk, const glm::ivec3& origin, const glm::ivec3& base,
                       mge::Pcg32& rng) const;

        SandboxBlocks m_blocks;
        TerrainSettings m_settings;
        // One noise instance per "layer" (distinct seeds so the fields don't correlate)
        mge::Noise m_height, m_mountains, m_temperature, m_moisture, m_caveA, m_caveB;
    };
}
