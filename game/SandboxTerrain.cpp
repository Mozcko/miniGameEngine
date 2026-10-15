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
        if (height > kMaxSurfaceHeight) height = kMaxSurfaceHeight;
        return height;
    }

    SandboxTerrain::Column SandboxTerrain::column(int worldX, int worldZ) const {
        // Generador determinista por columna (stream 1 = terreno)
        mge::Pcg32 rng(mge::hash2i(worldX, worldZ, m_seed), 1);
        Column col{terrainHeight(worldX, worldZ), m_blocks.grass};
        if (col.height <= kWaterLevel + 1) {
            col.surface = m_blocks.sand; // Playas
        } else if (rng.range(0, 100) < 5) {
            col.surface = m_blocks.gravel; // Parches de grava
        }
        // Piscina de lava (rara)
        if (rng.range(0, 200) == 0 && col.height > kWaterLevel) col.surface = m_blocks.lava;
        return col;
    }

    void SandboxTerrain::generate(const mge::ChunkCoord& coord, ChunkData& chunk) const {
        const glm::ivec3 origin = mge::chunkOrigin(coord);

        // PASADA 1: terreno base (y del mundo: sirve para chunks a cualquier altura)
        for (int x = 0; x < kChunkSize; x++) {
            for (int z = 0; z < kChunkSize; z++) {
                const Column col = column(origin.x + x, origin.z + z);
                for (int y = 0; y < kChunkSize; y++) {
                    const int worldY = origin.y + y;
                    BlockId id = mge::kAir;
                    if (worldY < 0) {
                        id = m_blocks.stone; // Debajo del mundo: roca
                    } else if (worldY > col.height) {
                        if (worldY <= kWaterLevel) id = m_blocks.water;
                    } else if (worldY == col.height) {
                        id = col.surface;
                    } else if (worldY > col.height - 3) {
                        id = m_blocks.dirt;
                    } else {
                        id = m_blocks.stone;
                    }
                    chunk.set(x, y, z, id);
                }
            }
        }

        // PASADA 2: decoración. Se recorren también las columnas a kTreeRadius fuera del chunk: sus árboles
        // pueden meter hojas aquí. El orden es el del mundo (x, luego z), igual en todos los chunks, así que
        // donde dos árboles se tocan el resultado es el mismo lo genere quien lo genere.
        for (int wx = origin.x - kTreeRadius; wx < origin.x + kChunkSize + kTreeRadius; wx++) {
            for (int wz = origin.z - kTreeRadius; wz < origin.z + kChunkSize + kTreeRadius; wz++) {
                const Column col = column(wx, wz);
                if (col.surface != m_blocks.grass || col.height <= kWaterLevel) continue;

                // Misma columna, otro stream (2 = decoración)
                mge::Pcg32 rng(mge::hash2i(wx, wz, m_seed), 2);
                const int r = rng.range(0, 100);
                const glm::ivec3 base(wx, col.height + 1, wz); // Encima de la superficie (mundo)

                if (r < 2) { // 2 % árbol
                    placeTree(chunk, origin, base, rng);
                } else if (r < 6) { // 2 % rosa, 2 % margarita: solo en su columna
                    chunk.set(base - origin, r < 4 ? m_blocks.rose : m_blocks.flower);
                }
            }
        }
    }

    void SandboxTerrain::placeTree(ChunkData& chunk, const glm::ivec3& origin, const glm::ivec3& base,
                                   mge::Pcg32& rng) const {
        const int treeHeight = rng.range(4, 6); // 4 o 5

        // Copa: rombo de radio 2 sin las esquinas lejanas; solo ocupa aire. Se escribe la parte que cae
        // dentro de este chunk (ChunkData::set ignora lo de afuera).
        for (int y = base.y + treeHeight - 2; y <= base.y + treeHeight + 1; y++) {
            for (int dx = -kTreeRadius; dx <= kTreeRadius; dx++) {
                for (int dz = -kTreeRadius; dz <= kTreeRadius; dz++) {
                    if (std::abs(dx) + std::abs(dz) >= 4) continue;
                    const glm::ivec3 local = glm::ivec3(base.x + dx, y, base.z + dz) - origin;
                    if (ChunkData::inside(local.x, local.y, local.z) && chunk.get(local) == mge::kAir)
                        chunk.set(local, m_blocks.leaves);
                }
            }
        }

        // Tronco
        for (int i = 0; i < treeHeight; i++) chunk.set(base + glm::ivec3(0, i, 0) - origin, m_blocks.log);
    }
}
