#pragma once

#include <game/SandboxBlocks.h>
#include <mge/core/Random.h>
#include <mge/voxel/VoxelWorld.h>

#include <cstdint>

namespace sandbox {

    // Terreno del sandbox: olas de sin/cos, playas, grava, lava y decoración (árboles, flores).
    // Determinista por (coord, semilla) y sin estado: cada columna usa su propio Pcg32, y los árboles
    // de columnas vecinas (incluso de otro chunk) se reconstruyen desde su columna. Así un árbol cruza
    // bordes de chunk entero y el mundo no depende del orden de carga.
    class SandboxTerrain : public mge::TerrainGenerator {
    public:
        static constexpr std::uint32_t kDefaultSeed = 1337;
        static constexpr int kWaterLevel = 5;
        static constexpr int kMaxSurfaceHeight = mge::kChunkSize - 1;
        static constexpr int kTreeRadius = 2; // Copa: hasta 2 bloques del tronco

        explicit SandboxTerrain(const SandboxBlocks& blocks, std::uint32_t seed = kDefaultSeed);

        void generate(const mge::ChunkCoord& coord, mge::ChunkData& chunk) const override;

        // Altura de la superficie en la columna (x, z) del mundo, en [1, kMaxSurfaceHeight]
        static int terrainHeight(int worldX, int worldZ);

    private:
        // Lo que decide la pasada de terreno para una columna (función pura de (x, z, semilla))
        struct Column {
            int height;
            mge::BlockId surface; // Bloque en y == height (ya con la lava aplicada)
        };
        Column column(int worldX, int worldZ) const;

        void placeTree(mge::ChunkData& chunk, const glm::ivec3& origin, const glm::ivec3& base,
                       mge::Pcg32& rng) const;

        SandboxBlocks m_blocks;
        std::uint32_t m_seed;
    };
}
