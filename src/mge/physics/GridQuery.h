#pragma once

#include <mge/voxel/VoxelWorld.h>

#include <functional>
#include <glm/glm.hpp>

namespace mge {

    // Query to determine whether a block in integer world coordinates is solid
    using SolidQuery = std::function<bool(const glm::ivec3&)>;

    // Creates a SolidQuery from a VoxelWorld reference
    inline SolidQuery solidQuery(const VoxelWorld& world) {
        return [&world](const glm::ivec3& block) { return world.isSolid(block); };
    }

} // namespace mge
