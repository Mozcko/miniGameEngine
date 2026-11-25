#pragma once

#include <mge/voxel/Chunk.h>

#include <vector>

namespace mge {

    class VoxelWorld;

    // Copy of a chunk plus a 1-block border from its neighbours (18³ IDs, ~11 KB): everything the mesher
    // needs to read (visible faces and ambient occlusion). Captured on the main thread and can be meshed
    // on another thread without touching VoxelWorld. Local coordinates -1 to 16; unloaded neighbour = air.
    class ChunkSnapshot {
    public:
        static constexpr int kPaddedSize = kChunkSize + 2;

        // Invalid (valid() == false) if the chunk is not loaded
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
