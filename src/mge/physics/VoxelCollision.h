#pragma once

#include <mge/core/math/Geometry.h>
#include <mge/physics/GridQuery.h>
#include <mge/voxel/VoxelWorld.h>

#include <glm/glm.hpp>

namespace mge {

    struct MoveOptions {
        float stepHeight = 0.0f; // Maximum auto-climb step height (0 = disabled). E.g. 0.6 for a character
        float skin = 1e-4f; // Separation kept from walls (avoids touching due to floating-point rounding)
    };

    struct MoveResult {
        Aabb box;                  // Final box
        glm::vec3 moved{0.0f};     // Applied displacement (<= requested on each axis)
        glm::bvec3 blocked{false}; // Axes where collision occurred
        bool grounded = false;     // Collided while moving along -Y (supported by ground)
        bool stepped = false;      // Climbed a step
    };

    // Moves `box` by `delta` against solid blocks: axis-by-axis sweep (Y, then X, then Z) with time
    // of impact, preventing tunnelling through walls regardless of how large delta is.
    MoveResult moveAabb(const Aabb& box, const glm::vec3& delta, const SolidQuery& isSolid,
                        const MoveOptions& options = {});
    MoveResult moveAabb(const VoxelWorld& world, const Aabb& box, const glm::vec3& delta,
                        const MoveOptions& options = {});

} // namespace mge
