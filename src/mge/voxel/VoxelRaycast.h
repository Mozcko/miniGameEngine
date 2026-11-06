#pragma once

#include <mge/core/math/Geometry.h>
#include <mge/voxel/Block.h>
#include <mge/voxel/Chunk.h>
#include <mge/voxel/VoxelWorld.h>

#include <optional>

#include <glm/glm.hpp>

namespace mge {

    // Información del impacto de un rayo contra un bloque sólido del mundo voxel.
    struct VoxelRaycastHit {
        glm::ivec3 block{0};             // Coordenadas enteras del bloque impactado
        glm::ivec3 normal{0};            // Normal entera de la cara impactada (+X, -X, +Y, -Y, +Z, -Z)
        float distance = 0.0f;           // Distancia paramétrica t a lo largo del rayo
        BlockId blockId = kAir;          // Tipo de bloque impactado
        glm::vec3 point{0.0f};           // Punto exacto de impacto en coordenadas de mundo
        BlockFace face = BlockFace::Top; // Cara del bloque que recibió el impacto

        // Vector normal en punto flotante
        glm::vec3 faceNormal() const { return glm::vec3(normal); }
    };

    // Traza un rayo a través del mundo voxel usando el algoritmo DDA (Amanatides-Woo).
    // Avanza celda por celda hasta encontrar un bloque sólido o superar maxDistance.
    // Devuelve nullopt si no hubo impacto dentro del rango.
    // Si el origen del rayo ya está dentro de un bloque sólido, devuelve un impacto en t = 0 con normal (0,
    // 0, 0).
    std::optional<VoxelRaycastHit> raycastVoxels(const VoxelWorld& world, const Ray& ray,
                                                 float maxDistance = 100.0f);

} // namespace mge
