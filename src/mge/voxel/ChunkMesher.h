#pragma once

#include <mge/procgen/MeshData.h>
#include <mge/voxel/ChunkSnapshot.h>
#include <mge/voxel/VoxelWorld.h>

namespace mge {

    struct ChunkMeshOptions {
        TextureAtlas atlas; // For blocks with tiles; colour-only blocks have UV (0, 0)
        // Merges equal neighbouring faces into a single quad (colour-only blocks, untextured: atlas tiles
        // cannot repeat across a large quad until the new renderer, M3)
        bool greedy = true;
        // Per-vertex ambient occlusion: darkens vertex colour in corners and edges (0 to 3 opaque
        // neighbours). With greedy enabled, only faces with the same occlusion on all 4 corners are merged.
        bool ambientOcclusion = false;
        float aoStrength = 0.5f; // Colour in an enclosed corner = 1 - aoStrength
        // Textured blocks: false (default) = white vertex colour (texture as-is, plus AO);
        // true = texture is tinted with BlockDesc::color. Untextured blocks always use their colour.
        bool tintTextured = false;
        // Skirts on the 4 side borders (±X, ±Z) for LOD > 0 chunks (#61). Hides cracks between different
        // LODs.
        bool lodSkirts = true;
    };

    // Mesh of a chunk in local coordinates (0..16): draw with model = translate(chunkOrigin).
    // One mesh per render pass (BlockDesc::renderLayer): opaque (solids), cutout (leaves, plants:
    // alpha test), and transparent (translucent: water, glass, lava; alpha blending).
    struct ChunkMesh {
        MeshData opaque;
        MeshData cutout;
        MeshData transparent;
        // Chunk-local centroid of each transparent quad (1 per 6 indices)
        std::vector<glm::vec3> transparentFaceCentroids;
    };

    // Reorders quad indices (6 indices = 2 triangles per quad) back-to-front relative to
    // cameraLocal (farthest first), using per-face centroids. If sizes do not match or
    // there are fewer than two quads, returns a copy of indices unchanged.
    std::vector<std::uint32_t> sortQuadsBackToFront(const std::vector<std::uint32_t>& indices,
                                                    const std::vector<glm::vec3>& centroids,
                                                    const glm::vec3& cameraLocal);

    // Builds the mesh for a loaded chunk (empty if not loaded). Only emits visible faces:
    // a face is hidden if the neighbour is an opaque cube, or if both are the same transparent block
    // (water against water). Neighbours are read from the world, including across chunk borders; an
    // unloaded neighbour chunk counts as air (VoxelWorld marks the chunk dirty when that neighbour loads).
    ChunkMesh buildChunkMesh(const VoxelWorld& world, const ChunkCoord& coord,
                             const ChunkMeshOptions& options = {});
    // Same, from a snapshot copy (ChunkSnapshot::capture). Only reads the snapshot and the registry (which
    // must not change in the meantime): safe to call from another thread (JobSystem).
    ChunkMesh buildChunkMesh(const ChunkSnapshot& snapshot, const BlockRegistry& registry,
                             const ChunkMeshOptions& options = {});
}
