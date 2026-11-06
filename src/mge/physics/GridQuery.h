#pragma once

#include <mge/voxel/VoxelWorld.h>

#include <functional>
#include <glm/glm.hpp>

namespace mge {

    // Consulta para determinar si un bloque en coordenadas enteras de mundo es sólido
    using SolidQuery = std::function<bool(const glm::ivec3&)>;

    // Genera un SolidQuery a partir de una referencia a VoxelWorld
    inline SolidQuery solidQuery(const VoxelWorld& world) {
        return [&world](const glm::ivec3& block) { return world.isSolid(block); };
    }

} // namespace mge
