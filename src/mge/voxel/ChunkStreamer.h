#pragma once

#include <mge/core/Jobs.h>
#include <mge/voxel/ChunkLod.h>
#include <mge/voxel/ChunkMesher.h>
#include <mge/voxel/VoxelWorld.h>

#include <array>
#include <cstddef>
#include <future>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

namespace mge {

    struct StreamingSettings {
        int loadRadius = 5;   // Columns (Chebyshev distance in X/Z) loaded around the center
        int meshRadius = 4;   // Meshed columns (< loadRadius so neighbours already have data)
        int unloadMargin = 2; // Hysteresis: unloaded only beyond loadRadius + unloadMargin
        int minChunkY = 0;    // Vertical chunk range (inclusive)
        int maxChunkY = 0;
        int maxJobsInFlight = 32;     // Concurrent jobs (generate + mesh) in the JobSystem
        int maxResultsPerUpdate = 16; // Generations and meshes applied/returned per update
        // 0 = no limit. If more chunks are loaded, the farthest outside the mesh radius are unloaded
        std::size_t maxLoadedChunks = 0;
        ChunkMeshOptions meshOptions;

        // LOD ring radii (horizontal Chebyshev distance where each LOD ends).
        // For example {6, 12, 24} -> LOD 0 up to radius 6, LOD 1 up to radius 12, LOD 2 up to radius 24.
        // 0 (or lodRadius[0] == 0) disables LOD (previous behaviour: all LOD 0 up to meshRadius).
        // Note: LOD 0 hysteresis may keep a chunk at LOD 0 one ring farther than
        // lodRadius[0] (up to lodRadius[0] + 1) to avoid remeshing oscillations at the boundary.
        std::array<int, 3> lodRadius = {0, 0, 0};

        bool lodEnabled() const { return lodRadius[0] > 0; }

        int maxMeshRadius() const {
            if (!lodEnabled()) return meshRadius;
            if (lodRadius[2] > 0) return lodRadius[2];
            if (lodRadius[1] > 0) return lodRadius[1];
            return lodRadius[0];
        }
    };

    struct StreamingStats {
        std::size_t loaded = 0;          // Chunks with data
        std::size_t pendingGenerate = 0; // Generation jobs in flight
        std::size_t pendingMesh = 0;     // Meshing jobs in flight
        std::size_t pooled = 0;          // Chunks in VoxelWorld's pool
        std::size_t discarded =
            0; // Results discarded as stale (chunk edited, unloaded, or LOD changed)
        std::size_t cpuBytes() const { return loaded * sizeof(ChunkData); }
    };

    // Mesh returned by the streamer for a chunk
    struct StreamMesh {
        ChunkCoord coord;
        ChunkMesh mesh;
        int lod = 0;
    };

    // What the game needs to do after an update
    struct StreamUpdate {
        std::vector<StreamMesh> meshes;   // Upload to GPU with its LOD level (replaces previous)
        std::vector<ChunkCoord> unloaded; // Free their GPU meshes
    };

    // Loads, meshes, and unloads chunks around a moving point (#57). With a JobSystem, generates and
    // meshes on worker threads (#56): the main thread only inserts results and captures snapshots, with a
    // per-update budget, so crossing into a new chunk does not stall the frame. Without a JobSystem, runs
    // the same logic on the main thread (tests, headless programs), with the same per-update budget.
    //   - Nearest first; memory stays bounded by the unload radius (and maxLoadedChunks).
    //   - Stale results (chunk unloaded, edited after requesting the mesh, or outdated LOD) are discarded.
    // VoxelWorld (and its registry) must outlive the streamer; Application waits for jobs before onShutdown.
    class ChunkStreamer {
    public:
        ChunkStreamer(VoxelWorld& world, StreamingSettings settings, JobSystem* jobs = nullptr);
        ~ChunkStreamer();
        ChunkStreamer(const ChunkStreamer&) = delete;
        ChunkStreamer& operator=(const ChunkStreamer&) = delete;

        // center: camera/player chunk (only X and Z matter: full vertical columns are loaded)
        StreamUpdate update(const ChunkCoord& center);

        StreamingStats stats() const;
        const StreamingSettings& settings() const { return m_settings; }
        void setSettings(StreamingSettings settings);

        // Clears the LOD map and marks all loaded chunks dirty to force remeshing
        void remeshAll();

        // Current LOD level assigned to the chunk (-1 if not meshed)
        int chunkLod(const ChunkCoord& coord) const;

        // Target LOD level for a coordinate given the center and its current LOD (with 1-chunk hysteresis)
        int targetLod(const ChunkCoord& coord, const ChunkCoord& center, int currentLod = -1) const;

        // Is everything corresponding to center already loaded and meshed? (tests, loading screens)
        bool idle() const { return m_generating.empty() && m_meshing.empty() && m_lastUpdateIdle; }

        // Hook called on the main thread before inserting and meshing a newly generated chunk
        void onChunkInserted(std::function<void(const ChunkCoord&, ChunkData&)> callback) {
            m_onChunkInserted = std::move(callback);
        }

    private:
        static int columnDistance(const ChunkCoord& a, const ChunkCoord& b);
        bool neighboursReady(const ChunkCoord& coord) const;
        void collectGenerated(StreamUpdate& out, const ChunkCoord& center, int& budget);
        void collectMeshes(StreamUpdate& out, const ChunkCoord& center, int& budget);
        void unloadFar(StreamUpdate& out, const ChunkCoord& center);
        void requestGeneration(const ChunkCoord& center, int& budget);
        void requestMeshes(StreamUpdate& out, const ChunkCoord& center, int& budget);

        struct PendingMesh {
            std::future<ChunkMesh> future;
            int requestedLod = 0;
        };

        VoxelWorld& m_world;
        StreamingSettings m_settings;
        JobSystem* m_jobs;
        std::unordered_map<ChunkCoord, std::future<ChunkData>, ChunkCoordHash> m_generating;
        std::unordered_map<ChunkCoord, PendingMesh, ChunkCoordHash> m_meshing;
        std::unordered_map<ChunkCoord, int, ChunkCoordHash> m_chunkLod;
        std::function<void(const ChunkCoord&, ChunkData&)> m_onChunkInserted;
        std::size_t m_discarded = 0;
        bool m_lastUpdateIdle = false;
    };

} // namespace mge
