#pragma once

#include <mge/core/math/Geometry.h>
#include <mge/physics/GridQuery.h>
#include <mge/voxel/VoxelWorld.h>

#include <glm/glm.hpp>

namespace mge {

    struct MoveOptions {
        float stepHeight = 0.0f; // Escalón máximo que se sube solo (0 = no subir). Ej: 0.6 para un personaje
        float skin = 1e-4f; // Separación que se deja contra la pared (evita quedar "tocando" por redondeo)
    };

    struct MoveResult {
        Aabb box;                  // Caja final
        glm::vec3 moved{0.0f};     // Desplazamiento aplicado (<= el pedido en cada eje)
        glm::bvec3 blocked{false}; // Ejes donde se chocó
        bool grounded = false;     // Chocó moviéndose hacia -Y (está apoyado)
        bool stepped = false;      // Subió un escalón
    };

    // Mueve la caja `delta` contra los bloques sólidos: barrido eje por eje (Y, luego X, luego Z) con tiempo
    // de impacto, así no atraviesa paredes por más grande que sea delta.
    MoveResult moveAabb(const Aabb& box, const glm::vec3& delta, const SolidQuery& isSolid,
                        const MoveOptions& options = {});
    MoveResult moveAabb(const VoxelWorld& world, const Aabb& box, const glm::vec3& delta,
                        const MoveOptions& options = {});

} // namespace mge
