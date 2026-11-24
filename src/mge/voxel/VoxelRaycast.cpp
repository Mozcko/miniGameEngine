#include <mge/voxel/VoxelRaycast.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace mge {

    namespace {
        // Maps the integer face normal to the BlockFace enum
        BlockFace faceFromNormal(const glm::ivec3& normal) {
            if (normal.x < 0) return BlockFace::Left;
            if (normal.x > 0) return BlockFace::Right;
            if (normal.y < 0) return BlockFace::Bottom;
            if (normal.y > 0) return BlockFace::Top;
            if (normal.z < 0) return BlockFace::Back;
            return BlockFace::Front; // normal.z > 0 or default case
        }
    } // namespace

    std::optional<VoxelRaycastHit> raycastVoxels(const VoxelWorld& world, const Ray& ray, float maxDistance) {
        return raycastVoxels(world, ray, maxDistance,
                             [&world](BlockId id) { return world.registry().isSolid(id); });
    }

    std::optional<VoxelRaycastHit> raycastVoxels(const VoxelWorld& world, const Ray& ray, float maxDistance,
                                                 const std::function<bool(BlockId)>& pickable) {
        if (std::isnan(maxDistance) || maxDistance < 0.0f) {
            return std::nullopt;
        }
        if (std::isinf(maxDistance) || maxDistance > kMaxRaycastDistance) {
            maxDistance = kMaxRaycastDistance;
        }

        glm::ivec3 currentBlock = blockAt(ray.origin);

        const auto isTarget = [&world, &pickable](const glm::ivec3& block) {
            const BlockId id = world.getBlock(block);
            return pickable ? pickable(id) : world.registry().isSolid(id);
        };

        // If the ray starts inside a target block, report an immediate hit at distance 0
        if (isTarget(currentBlock)) {
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

        // Amanatides-Woo parameters per Cartesian axis
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

        // Conservative step limit to guarantee termination on extreme values
        const int maxSteps = static_cast<int>(std::ceil(maxDistance * 3.5f)) + 32;
        int lastStepAxis = -1;

        for (int s = 0; s < maxSteps; ++s) {
            // Identify the axis along which the ray crosses into the next block first.
            // Tie-breaking prefers X, then Y, then Z (deterministic <=).
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

            // If the next boundary is beyond the requested distance, there is no hit
            if (t > maxDistance) {
                break;
            }

            currentBlock[hitAxis] += step[hitAxis];
            lastStepAxis = hitAxis;

            // Upon entering the new block, check if it matches the predicate
            if (isTarget(currentBlock)) {
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
