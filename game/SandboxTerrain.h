#pragma once

#include <game/SandboxBlocks.h>
#include <mge/core/Random.h>
#include <mge/voxel/VoxelWorld.h>

#include <cstdint>

namespace sandbox {

    // Terreno del sandbox: olas de sin/cos, playas, grava, lava y decoración (árboles, flores).
    // Determinista por (coord, semilla): cada columna usa su propio Pcg32 (sin estado global).
    // Por ahora el mundo tiene un solo chunk de alto (y = 0..15); los demás quedan vacíos.
    class SandboxTerrain : public mge::TerrainGenerator {
    public:
        static constexpr std::uint32_t kDefaultSeed = 1337;
        static constexpr int kWaterLevel = 5;
        static constexpr int kWorldHeight = mge::kChunkSize;

        explicit SandboxTerrain(const SandboxBlocks& blocks, std::uint32_t seed = kDefaultSeed);

        void generate(const mge::ChunkCoord& coord, mge::ChunkData& chunk) const override;

        // Altura de la superficie en la columna (x, z) del mundo, en [1, kWorldHeight - 1]
        static int terrainHeight(int worldX, int worldZ);

    private:
        void placeTree(mge::ChunkData& chunk, int x, int y, int z, mge::Pcg32& rng) const;

        SandboxBlocks m_blocks;
        std::uint32_t m_seed;
    };
}
