#pragma once

#include <mge/core/math/Geometry.h>
#include <mge/voxel/Block.h>
#include <mge/voxel/Chunk.h>
#include <mge/voxel/VoxelWorld.h>

#include <functional>
#include <optional>

#include <glm/glm.hpp>

namespace mge {

    // Ray impact information against a solid block in the voxel world.
    struct VoxelRaycastHit {
        glm::ivec3 block{0};             // Integer coordinates of the hit block
        glm::ivec3 normal{0};            // Integer normal of the hit face (+X, -X, +Y, -Y, +Z, -Z)
        float distance = 0.0f;           // Parametric distance t along the ray
        BlockId blockId = kAir;          // Type of the hit block
        glm::vec3 point{0.0f};           // Exact impact point in world coordinates
        BlockFace face = BlockFace::Top; // Face of the block that was hit

        // Floating-point normal vector
        glm::vec3 faceNormal() const { return glm::vec3(normal); }
    };

    // Maximum allowed distance for raycasting in the voxel world
    constexpr float kMaxRaycastDistance = 4096.0f;

    // Casts a ray through the voxel world using the DDA (Amanatides-Woo) algorithm.
    // Steps cell by cell until it finds a block matching the pickable predicate (or solid by
    // default) or exceeds maxDistance.
    // Returns nullopt if there is no hit within range or if maxDistance is NaN or negative.
    // Infinite values or values greater than kMaxRaycastDistance are clamped to kMaxRaycastDistance.
    // Tie-breaking on edges or corners prefers X over Y over Z (deterministic and documented <=).
    // If the ray origin is already inside a target block, returns a hit at t = 0 with normal (0, 0, 0).
    std::optional<VoxelRaycastHit> raycastVoxels(const VoxelWorld& world, const Ray& ray, float maxDistance,
                                                 const std::function<bool(BlockId)>& pickable);

    std::optional<VoxelRaycastHit> raycastVoxels(const VoxelWorld& world, const Ray& ray,
                                                 float maxDistance = 100.0f);

} // namespace mge
