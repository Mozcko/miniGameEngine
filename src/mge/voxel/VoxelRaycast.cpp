#include <mge/voxel/VoxelRaycast.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace mge {

    namespace {
        // Asocia la normal entera de la cara con el enum BlockFace
        BlockFace faceFromNormal(const glm::ivec3& normal) {
            if (normal.x < 0) return BlockFace::Left;
            if (normal.x > 0) return BlockFace::Right;
            if (normal.y < 0) return BlockFace::Bottom;
            if (normal.y > 0) return BlockFace::Top;
            if (normal.z < 0) return BlockFace::Back;
            return BlockFace::Front; // normal.z > 0 o caso por defecto
        }
    } // namespace

    std::optional<VoxelRaycastHit> raycastVoxels(const VoxelWorld& world, const Ray& ray, float maxDistance) {
        if (std::isnan(maxDistance) || maxDistance < 0.0f) {
            return std::nullopt;
        }
        if (std::isinf(maxDistance) || maxDistance > kMaxRaycastDistance) {
            maxDistance = kMaxRaycastDistance;
        }

        glm::ivec3 currentBlock = blockAt(ray.origin);

        // Si el rayo nace dentro de un bloque sólido, reporta impacto inmediato a distancia 0
        if (world.isSolid(currentBlock)) {
            VoxelRaycastHit hit;
            hit.block = currentBlock;
            hit.normal = glm::ivec3(0);
            hit.distance = 0.0f;
            hit.blockId = world.getBlock(currentBlock);
            hit.point = ray.origin;
            hit.face = BlockFace::Top;
            return hit;
        }

        if (maxDistance == 0.0f) {
            return std::nullopt;
        }

        constexpr float kEpsilon = 1e-8f;

        // Parámetros de Amanatides-Woo por cada eje cartesiano
        glm::ivec3 step(0);
        glm::vec3 tDelta(std::numeric_limits<float>::infinity());
        glm::vec3 tMax(std::numeric_limits<float>::infinity());

        for (int i = 0; i < 3; ++i) {
            const float dir = ray.direction[i];
            const float origin = ray.origin[i];
            const float blockCoord = static_cast<float>(currentBlock[i]);

            if (dir > kEpsilon) {
                step[i] = 1;
                tDelta[i] = 1.0f / dir;
                tMax[i] = std::max(0.0f, (blockCoord + 1.0f - origin) * tDelta[i]);
            } else if (dir < -kEpsilon) {
                step[i] = -1;
                tDelta[i] = 1.0f / -dir;
                tMax[i] = std::max(0.0f, (origin - blockCoord) * tDelta[i]);
            } else {
                step[i] = 0;
                tDelta[i] = std::numeric_limits<float>::infinity();
                tMax[i] = std::numeric_limits<float>::infinity();
            }
        }

        // Límite conservador de pasos para garantizar terminación ante valores extremos
        const int maxSteps = static_cast<int>(std::ceil(maxDistance * 3.5f)) + 32;
        int lastStepAxis = -1;

        for (int s = 0; s < maxSteps; ++s) {
            // Identificar el eje por el cual el rayo cruza antes al siguiente bloque.
            // Los desempates prefieren X, luego Y, luego Z (<= determinista).
            int hitAxis = 0;
            if (tMax.x <= tMax.y) {
                if (tMax.x <= tMax.z) {
                    hitAxis = 0;
                } else {
                    hitAxis = 2;
                }
            } else {
                if (tMax.y <= tMax.z) {
                    hitAxis = 1;
                } else {
                    hitAxis = 2;
                }
            }

            const float t = tMax[hitAxis];

            // Si el siguiente límite está más allá de la distancia pedida, no hay impacto
            if (t > maxDistance) {
                break;
            }

            currentBlock[hitAxis] += step[hitAxis];
            lastStepAxis = hitAxis;

            // Al entrar al nuevo bloque comprobamos si es sólido
            if (world.isSolid(currentBlock)) {
                VoxelRaycastHit hit;
                hit.block = currentBlock;
                hit.normal = glm::ivec3(0);
                hit.normal[lastStepAxis] = -step[lastStepAxis];
                hit.distance = t;
                hit.blockId = world.getBlock(currentBlock);
                hit.point = ray.at(t);
                hit.face = faceFromNormal(hit.normal);
                return hit;
            }

            tMax[hitAxis] += tDelta[hitAxis];
        }

        return std::nullopt;
    }

} // namespace mge
