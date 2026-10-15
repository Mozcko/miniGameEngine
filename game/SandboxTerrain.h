#pragma once

#include <game/SandboxBlocks.h>
#include <mge/core/Random.h>
#include <mge/procgen/Noise.h>
#include <mge/voxel/VoxelWorld.h>

#include <cstdint>

namespace sandbox {

    enum class Biome {
        Plains,    // Pasto, árboles y flores
        Desert,    // Arena (caliente y seco)
        Mountains, // Roca y grava, alto
        Shore,     // Arena junto al agua
    };

    struct TerrainSettings {
        std::uint32_t seed = 1337;
        bool caves = true; // Túneles con ruido 3D (nunca rompen el piso y = 0 ni la superficie)
    };

    // Terreno del sandbox con ruido (#27): altura por fbm de simplex + montañas (ruido "ridged" bajo una
    // máscara), biomas por temperatura y humedad, cuevas con ruido 3D y decoración (árboles, flores).
    // Determinista por (coord, semilla) y sin estado: cada columna es una función pura, y los árboles de
    // columnas vecinas (incluso de otro chunk) se reconstruyen desde su columna. Así un árbol cruza bordes
    // de chunk entero y el mundo no depende del orden de carga.
    class SandboxTerrain : public mge::TerrainGenerator {
    public:
        static constexpr std::uint32_t kDefaultSeed = 1337;
        static constexpr int kWorldChunksY = 4;                              // Chunks de alto
        static constexpr int kWorldHeight = kWorldChunksY * mge::kChunkSize; // 64 bloques
        static constexpr int kWaterLevel = 20;
        static constexpr int kMaxSurfaceHeight = kWorldHeight - 8; // Deja lugar a los árboles
        static constexpr int kTreeRadius = 2;                      // Copa: hasta 2 bloques del tronco

        explicit SandboxTerrain(const SandboxBlocks& blocks, TerrainSettings settings = {});

        void generate(const mge::ChunkCoord& coord, mge::ChunkData& chunk) const override;

        // Altura de la superficie en la columna (x, z) del mundo, en [1, kMaxSurfaceHeight]
        int surfaceHeight(int worldX, int worldZ) const { return column(worldX, worldZ).height; }
        Biome biome(int worldX, int worldZ) const { return column(worldX, worldZ).biome; }
        // ¿La cueva vacía este bloque? (solo bajo la superficie, nunca en y = 0)
        bool isCave(int worldX, int worldY, int worldZ, int surfaceHeight) const;

    private:
        // Lo que decide la pasada de terreno para una columna (función pura de (x, z, semilla))
        struct Column {
            int height;
            Biome biome;
            mge::BlockId surface; // Bloque en y == height
            mge::BlockId filler;  // Las capas de abajo de la superficie (tierra, arena, roca)
        };
        Column column(int worldX, int worldZ) const;

        void placeTree(mge::ChunkData& chunk, const glm::ivec3& origin, const glm::ivec3& base,
                       mge::Pcg32& rng) const;

        SandboxBlocks m_blocks;
        TerrainSettings m_settings;
        // Un ruido por "capa" (semillas distintas: los campos no se parecen entre sí)
        mge::Noise m_height, m_mountains, m_temperature, m_moisture, m_caveA, m_caveB;
    };
}
