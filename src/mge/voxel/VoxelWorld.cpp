#include <mge/voxel/VoxelWorld.h>

#include <utility>

namespace mge {

    VoxelWorld::VoxelWorld(const BlockRegistry& registry, std::unique_ptr<TerrainGenerator> generator)
        : m_registry(registry), m_generator(std::move(generator)) {}

    ChunkData& VoxelWorld::loadChunk(const ChunkCoord& coord) {
        auto it = m_chunks.find(coord);
        if (it != m_chunks.end()) return it->second->data;
        ChunkData data;
        if (m_generator) m_generator->generate(coord, data);
        return insertChunk(coord, data);
    }

    ChunkData& VoxelWorld::insertChunk(const ChunkCoord& coord, const ChunkData& data) {
        auto it = m_chunks.find(coord);
        if (it != m_chunks.end()) return it->second->data;
        auto entry = std::make_unique<Entry>();
        entry->data = data;
        ChunkData& stored = entry->data;
        m_chunks.emplace(coord, std::move(entry));
        markFaceNeighboursDirty(coord);
        return stored;
    }

    void VoxelWorld::unloadChunk(const ChunkCoord& coord) {
        m_chunks.erase(coord);
    }

    ChunkData* VoxelWorld::chunk(const ChunkCoord& coord) {
        auto it = m_chunks.find(coord);
        return it == m_chunks.end() ? nullptr : &it->second->data;
    }

    const ChunkData* VoxelWorld::chunk(const ChunkCoord& coord) const {
        auto it = m_chunks.find(coord);
        return it == m_chunks.end() ? nullptr : &it->second->data;
    }

    std::vector<ChunkCoord> VoxelWorld::loadedChunks() const {
        std::vector<ChunkCoord> coords;
        coords.reserve(m_chunks.size());
        for (const auto& pair : m_chunks) coords.push_back(pair.first);
        return coords;
    }

    BlockId VoxelWorld::getBlock(const glm::ivec3& block) const {
        const ChunkData* data = chunk(chunkOf(block));
        return data ? data->get(localOf(block)) : kAir;
    }

    bool VoxelWorld::setBlock(const glm::ivec3& block, BlockId id) {
        const ChunkCoord coord = chunkOf(block);
        ChunkData* data = chunk(coord);
        if (!data) return false;
        const glm::ivec3 local = localOf(block);
        data->set(local, id);
        markDirty(coord);

        // Bloque en un borde: la malla del vecino de ese lado también lo mira
        // (sin vecinos en diagonal: el AO de las esquinas puede quedar desactualizado hasta el próximo
        // mallado)
        for (int axis = 0; axis < 3; axis++) {
            if (local[axis] == 0) {
                ChunkCoord n = coord;
                (axis == 0 ? n.x : axis == 1 ? n.y : n.z) -= 1;
                markDirty(n);
            } else if (local[axis] == kChunkSize - 1) {
                ChunkCoord n = coord;
                (axis == 0 ? n.x : axis == 1 ? n.y : n.z) += 1;
                markDirty(n);
            }
        }
        return true;
    }

    void VoxelWorld::markFaceNeighboursDirty(const ChunkCoord& coord) {
        markDirty({coord.x - 1, coord.y, coord.z});
        markDirty({coord.x + 1, coord.y, coord.z});
        markDirty({coord.x, coord.y - 1, coord.z});
        markDirty({coord.x, coord.y + 1, coord.z});
        markDirty({coord.x, coord.y, coord.z - 1});
        markDirty({coord.x, coord.y, coord.z + 1});
    }

    bool VoxelWorld::isDirty(const ChunkCoord& coord) const {
        auto it = m_chunks.find(coord);
        return it != m_chunks.end() && it->second->dirty;
    }

    void VoxelWorld::markDirty(const ChunkCoord& coord) {
        auto it = m_chunks.find(coord);
        if (it != m_chunks.end()) it->second->dirty = true;
    }

    void VoxelWorld::clearDirty(const ChunkCoord& coord) {
        auto it = m_chunks.find(coord);
        if (it != m_chunks.end()) it->second->dirty = false;
    }

    std::vector<ChunkCoord> VoxelWorld::dirtyChunks() const {
        std::vector<ChunkCoord> coords;
        for (const auto& pair : m_chunks)
            if (pair.second->dirty) coords.push_back(pair.first);
        return coords;
    }
}
