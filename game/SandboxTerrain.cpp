#include <game/SandboxTerrain.h>

#include <cmath>
#include <cstdlib>

namespace sandbox {

    using mge::BlockId;
    using mge::ChunkData;
    using mge::kChunkSize;

    SandboxTerrain::SandboxTerrain(const SandboxBlocks& blocks, std::uint32_t seed)
        : m_blocks(blocks), m_seed(seed) {}

    int SandboxTerrain::terrainHeight(int worldX, int worldZ) {
        // Olas simples (el ruido llega en #27)
        const float heightVal = (std::sin(worldX * 0.2f) + std::cos(worldZ * 0.2f)) * 3.0f + 6.0f;
        int height = static_cast<int>(heightVal);
        if (height < 1) height = 1;
        if (height >= kWorldHeight) height = kWorldHeight - 1;
        return height;
    }

    void SandboxTerrain::generate(const mge::ChunkCoord& coord, ChunkData& chunk) const {
        if (coord.y != 0) return; // Un solo chunk de alto

        // PASADA 1: terreno base
        for (int x = 0; x < kChunkSize; x++) {
            for (int z = 0; z < kChunkSize; z++) {
                // Coordenadas globales para que el terreno coincida entre chunks
                const int worldX = coord.x * kChunkSize + x;
                const int worldZ = coord.z * kChunkSize + z;

                // Generador determinista por columna (stream 1 = terreno)
                mge::Pcg32 rng(mge::hash2i(worldX, worldZ, m_seed), 1);

                const int height = terrainHeight(worldX, worldZ);

                BlockId surface = m_blocks.grass;
                if (height <= kWaterLevel + 1) {
                    surface = m_blocks.sand; // Playas
                } else if (rng.range(0, 100) < 5) {
                    surface = m_blocks.gravel; // Parches de grava
                }

                // Piscina de lava (rara)
                const bool lavaPool = (rng.range(0, 200) == 0) && height > kWaterLevel;

                for (int y = 0; y < kChunkSize; y++) {
                    BlockId id = mge::kAir;
                    if (y > height) {
                        if (y <= kWaterLevel) id = m_blocks.water;
                    } else if (y == height) {
                        id = lavaPool ? m_blocks.lava : surface;
                    } else if (y > height - 3) {
                        id = m_blocks.dirt;
                    } else {
                        id = m_blocks.stone;
                    }
                    chunk.set(x, y, z, id);
                }
            }
        }

        // PASADA 2: decoración (árboles y flores), después del terreno para no pisarla
        for (int x = 0; x < kChunkSize; x++) {
            for (int z = 0; z < kChunkSize; z++) {
                const int worldX = coord.x * kChunkSize + x;
                const int worldZ = coord.z * kChunkSize + z;
                // Misma columna, otro stream (2 = decoración)
                mge::Pcg32 rng(mge::hash2i(worldX, worldZ, m_seed), 2);

                const int height = terrainHeight(worldX, worldZ);
                if (chunk.get(x, height, z) != m_blocks.grass || height <= kWaterLevel) continue;

                const int r = rng.range(0, 100);
                const int y = height + 1;
                if (y >= kChunkSize) continue;

                if (r < 2) { // 2 % árbol
                    placeTree(chunk, x, y, z, rng);
                } else if (r < 4) { // 2 % rosa
                    chunk.set(x, y, z, m_blocks.rose);
                } else if (r < 6) { // 2 % margarita
                    chunk.set(x, y, z, m_blocks.flower);
                }
            }
        }
    }

    void SandboxTerrain::placeTree(ChunkData& chunk, int x, int y, int z, mge::Pcg32& rng) const {
        const int treeHeight = rng.range(4, 6); // 4 o 5

        // Copa: rombo de radio 2 sin las esquinas lejanas. Solo dentro de este chunk (KI#6, #26)
        for (int ly = y + treeHeight - 2; ly <= y + treeHeight + 1; ly++) {
            for (int lx = x - 2; lx <= x + 2; lx++) {
                for (int lz = z - 2; lz <= z + 2; lz++) {
                    if (std::abs(lx - x) + std::abs(lz - z) >= 4) continue;
                    if (ChunkData::inside(lx, ly, lz) && chunk.get(lx, ly, lz) == mge::kAir)
                        chunk.set(lx, ly, lz, m_blocks.leaves);
                }
            }
        }

        // Tronco
        for (int i = 0; i < treeHeight; i++) chunk.set(x, y + i, z, m_blocks.log);
    }
}
