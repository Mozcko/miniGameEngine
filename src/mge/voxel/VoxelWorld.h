#pragma once

#include <mge/voxel/Block.h>
#include <mge/voxel/Chunk.h>

#include <memory>
#include <unordered_map>
#include <vector>

namespace mge {

    // Populates a newly created chunk. Must be deterministic (same coord → same blocks) and not depend
    // on which other chunks are loaded, so the world does not vary with load order.
    class TerrainGenerator {
    public:
        virtual ~TerrainGenerator() = default;
        virtual void generate(const ChunkCoord& coord, ChunkData& chunk) const = 0;
    };

    // Voxel world: loaded chunks (unbounded in size, including vertically), world-coordinate access,
    // and dirty flags to track which chunks need remeshing.
    class VoxelWorld {
    public:
        // The registry must outlive the world. Without a generator, new chunks start empty.
        explicit VoxelWorld(const BlockRegistry& registry,
                            std::unique_ptr<TerrainGenerator> generator = nullptr);

        const BlockRegistry& registry() const { return m_registry; }
        // The generator (nullptr if none). generate() is const and must be stateless: it can be called
        // from worker threads to generate chunks in parallel and insert them later with insertChunk.
        const TerrainGenerator* generator() const { return m_generator.get(); }

        // Creates and generates the chunk if it doesn't exist; if it already exists, returns it as-is.
        // The new chunk starts dirty, along with its 6 loaded neighbours: their meshes saw air on that side
        // and now there is data.
        ChunkData& loadChunk(const ChunkCoord& coord);
        // Inserts an already-generated chunk (e.g. from a JobSystem). If it already exists, keeps the
        // existing one. Same as loadChunk: starts dirty and marks its loaded neighbours dirty.
        ChunkData& insertChunk(const ChunkCoord& coord, const ChunkData& data);
        void unloadChunk(const ChunkCoord& coord);
        bool isLoaded(const ChunkCoord& coord) const { return m_chunks.count(coord) != 0; }
        ChunkData* chunk(const ChunkCoord& coord);
        const ChunkData* chunk(const ChunkCoord& coord) const;
        std::size_t chunkCount() const { return m_chunks.size(); }
        // Pool of unloaded chunks (their memory is reused when loading others): how many are pooled
        // and how many loads reused one
        std::size_t pooledCount() const { return m_pool.size(); }
        std::size_t reusedCount() const { return m_reused; }
        std::vector<ChunkCoord> loadedChunks() const;

        // World block coordinates. Unloaded chunk → air.
        BlockId getBlock(const glm::ivec3& block) const;
        BlockId getBlock(int x, int y, int z) const { return getBlock({x, y, z}); }
        // Returns false if the chunk is not loaded. Marks the chunk dirty and, if the block is on a border,
        // also marks the neighbour on that side dirty (its mesh inspects this block).
        bool setBlock(const glm::ivec3& block, BlockId id);
        bool isSolid(const glm::ivec3& block) const { return m_registry.isSolid(getBlock(block)); }

        // --- Dirty chunks: chunks whose contents (or a border neighbour's) changed ---
        bool isDirty(const ChunkCoord& coord) const;
        void markDirty(const ChunkCoord& coord); // Ignores unloaded chunks
        void clearDirty(const ChunkCoord& coord);
        std::vector<ChunkCoord> dirtyChunks() const;

    private:
        struct Entry {
            ChunkData data;
            bool dirty = true;
        };

        void markFaceNeighboursDirty(const ChunkCoord& coord);
        std::unique_ptr<Entry> takeEntry(); // From the pool if available, otherwise allocate a new one

        static constexpr std::size_t kMaxPooled = 256;
        std::vector<std::unique_ptr<Entry>> m_pool;
        std::size_t m_reused = 0;

        const BlockRegistry& m_registry;
        std::unique_ptr<TerrainGenerator> m_generator;
        // unique_ptr: stable addresses even if the map rehashes
        std::unordered_map<ChunkCoord, std::unique_ptr<Entry>, ChunkCoordHash> m_chunks;
    };
}
