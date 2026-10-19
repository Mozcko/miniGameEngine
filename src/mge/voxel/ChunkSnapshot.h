#pragma once

#include <mge/voxel/Chunk.h>

#include <vector>

namespace mge {

    class VoxelWorld;

    // Copia de un chunk más un borde de 1 bloque de sus vecinos (18³ ids, ~11 KB): todo lo que el mesher
    // necesita leer (caras visibles y oclusión ambiental). Se toma en el hilo principal y se puede mallar
    // en otro hilo sin tocar el VoxelWorld. Coordenadas locales de -1 a 16; vecino sin cargar = aire.
    class ChunkSnapshot {
    public:
        static constexpr int kPaddedSize = kChunkSize + 2;

        // Inválido (valid() == false) si el chunk no está cargado
        static ChunkSnapshot capture(const VoxelWorld& world, const ChunkCoord& coord);

        bool valid() const { return !m_blocks.empty(); }
        const ChunkCoord& coord() const { return m_coord; }

        BlockId get(int x, int y, int z) const { return m_blocks[index(x, y, z)]; }
        BlockId get(const glm::ivec3& local) const { return get(local.x, local.y, local.z); }

    private:
        static std::size_t index(int x, int y, int z) {
            return static_cast<std::size_t>(((y + 1) * kPaddedSize + (z + 1)) * kPaddedSize + (x + 1));
        }

        ChunkCoord m_coord;
        std::vector<BlockId> m_blocks;
    };
}
