#include <mge/voxel/Chunk.h>

namespace mge {

    bool ChunkData::isEmpty() const {
        for (BlockId id : m_blocks)
            if (id != kAir) return false;
        return true;
    }
}
