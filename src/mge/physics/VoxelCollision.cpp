#include <mge/physics/VoxelCollision.h>

#include <mge/core/Log.h>

#include <algorithm>
#include <atomic>
#include <cmath>

namespace mge {

    namespace {
        constexpr float kTiny = 1e-5f;
        constexpr float kMaxDelta = 256.0f;

        // Checks if the box already overlaps the block's interior volume at the start of the step
        bool boxOverlapsBlock(const Aabb& box, const glm::ivec3& block, float tiny) {
            for (int i = 0; i < 3; ++i) {
                const float blockMin = static_cast<float>(block[i]);
                const float blockMax = blockMin + 1.0f;
                if (box.max[i] <= blockMin + tiny || box.min[i] >= blockMax - tiny) {
                    return false;
                }
            }
            return true;
        }

        // Moves the box along a single axis while detecting collisions against solid blocks
        void moveSingleAxis(Aabb& currentBox, int axis, float deltaVal, const SolidQuery& isSolid, float skin,
                            bool& outBlocked, bool& outGrounded) {
            outBlocked = false;
            outGrounded = false;

            if (std::abs(deltaVal) < 1e-9f) {
                return;
            }

            // Swept volume along the current axis
            glm::vec3 sweptMin = currentBox.min;
            glm::vec3 sweptMax = currentBox.max;
            if (deltaVal > 0.0f) {
                sweptMax[axis] += deltaVal;
            } else {
                sweptMin[axis] += deltaVal;
            }

            // Integer block range intersecting the swept volume
            const int minX = static_cast<int>(std::floor(sweptMin.x));
            const int maxX = std::max(minX, static_cast<int>(std::floor(sweptMax.x - kTiny)));
            const int minY = static_cast<int>(std::floor(sweptMin.y));
            const int maxY = std::max(minY, static_cast<int>(std::floor(sweptMax.y - kTiny)));
            const int minZ = static_cast<int>(std::floor(sweptMin.z));
            const int maxZ = std::max(minZ, static_cast<int>(std::floor(sweptMax.z - kTiny)));

            float allowed = deltaVal;

            if (deltaVal > 0.0f) {
                for (int y = minY; y <= maxY; ++y) {
                    for (int z = minZ; z <= maxZ; ++z) {
                        for (int x = minX; x <= maxX; ++x) {
                            const glm::ivec3 blockPos(x, y, z);
                            if (!isSolid(blockPos)) {
                                continue;
                            }
                            // Ignore blocks the box already overlapped before starting this axis
                            if (boxOverlapsBlock(currentBox, blockPos, kTiny)) {
                                continue;
                            }
                            const float blockMin = static_cast<float>(blockPos[axis]);
                            float limit = blockMin - currentBox.max[axis] - skin;
                            if (limit < 0.0f) {
                                limit = 0.0f;
                            }
                            allowed = std::min(allowed, limit);
                        }
                    }
                }
            } else {
                for (int y = minY; y <= maxY; ++y) {
                    for (int z = minZ; z <= maxZ; ++z) {
                        for (int x = minX; x <= maxX; ++x) {
                            const glm::ivec3 blockPos(x, y, z);
                            if (!isSolid(blockPos)) {
                                continue;
                            }
                            if (boxOverlapsBlock(currentBox, blockPos, kTiny)) {
                                continue;
                            }
                            const float blockMax = static_cast<float>(blockPos[axis] + 1);
                            float limit = blockMax - currentBox.min[axis] + skin;
                            if (limit > 0.0f) {
                                limit = 0.0f;
                            }
                            allowed = std::max(allowed, limit);
                        }
                    }
                }
            }

            if (allowed != deltaVal) {
                outBlocked = true;
                if (axis == 1 && deltaVal < 0.0f) {
                    outGrounded = true;
                }
            }

            currentBox.min[axis] += allowed;
            currentBox.max[axis] += allowed;
        }

        // Executes the standard sequential movement in Y, X, Z order
        MoveResult moveSequence(const Aabb& initialBox, const glm::vec3& deltaVec, const SolidQuery& isSolid,
                                float skin) {
            MoveResult result;
            result.box = initialBox;

            // Strict resolution order: Y, then X, then Z
            const int axes[3] = {1, 0, 2};
            for (int axis : axes) {
                bool blockedAxis = false;
                bool groundedAxis = false;
                moveSingleAxis(result.box, axis, deltaVec[axis], isSolid, skin, blockedAxis, groundedAxis);
                if (blockedAxis) {
                    result.blocked[axis] = true;
                }
                if (groundedAxis) {
                    result.grounded = true;
                }
            }

            result.moved = result.box.min - initialBox.min;
            return result;
        }

        // Checks if the box is supported by the ground at the start using a -skin * 2 probe along Y
        bool isGroundedProbe(const Aabb& box, const SolidQuery& isSolid, float skin) {
            Aabb probeBox = box;
            bool blockedY = false;
            bool grounded = false;
            moveSingleAxis(probeBox, 1, -skin * 2.0f, isSolid, skin, blockedY, grounded);
            return blockedY;
        }
    } // namespace

    MoveResult moveAabb(const Aabb& box, const glm::vec3& delta, const SolidQuery& isSolid,
                        const MoveOptions& options) {
        if (box.isEmpty() || std::isnan(box.min.x) || std::isnan(box.min.y) || std::isnan(box.min.z) ||
            std::isnan(box.max.x) || std::isnan(box.max.y) || std::isnan(box.max.z) || std::isnan(delta.x) ||
            std::isnan(delta.y) || std::isnan(delta.z)) {
            MoveResult res;
            res.box = box;
            return res;
        }

        const float skin = (std::isnan(options.skin) || options.skin < 0.0f) ? 1e-4f : options.skin;

        // Clamp each delta component to prevent excessive scans on spurious deltas
        glm::vec3 clampedDelta = delta;
        for (int i = 0; i < 3; ++i) {
            if (std::abs(clampedDelta[i]) > kMaxDelta) {
                static std::atomic<bool> warnedClamp{false};
                if (!warnedClamp.exchange(true, std::memory_order_relaxed)) {
                    MGE_LOG_WARN("Physics", "moveAabb: delta clamped to ±256 blocks");
                }
                clampedDelta[i] = std::clamp(clampedDelta[i], -kMaxDelta, kMaxDelta);
            }
        }

        const bool groundedAtStart = isGroundedProbe(box, isSolid, skin);

        // Standard Y -> X -> Z movement
        MoveResult normalResult = moveSequence(box, clampedDelta, isSolid, skin);

        // Step-up: only if stepHeight > 0, delta.y <= 0, horizontal move was blocked, and grounded at start
        const bool horizontalBlocked = normalResult.blocked.x || normalResult.blocked.z;
        if (options.stepHeight > 0.0f && clampedDelta.y <= 0.0f && horizontalBlocked && groundedAtStart) {
            Aabb stepBox = box;
            bool blocked = false;
            bool grounded = false;

            // 1. Move up by stepHeight
            moveSingleAxis(stepBox, 1, options.stepHeight, isSolid, skin, blocked, grounded);

            // 2. Move along X
            bool blockedX = false;
            moveSingleAxis(stepBox, 0, clampedDelta.x, isSolid, skin, blockedX, grounded);

            // 3. Move along Z
            bool blockedZ = false;
            moveSingleAxis(stepBox, 2, clampedDelta.z, isSolid, skin, blockedZ, grounded);

            // 4. Move down by (stepHeight + |delta.y|)
            bool blockedDownY = false;
            bool groundedDown = false;
            const float downDist = -(options.stepHeight + std::abs(clampedDelta.y));
            moveSingleAxis(stepBox, 1, downDist, isSolid, skin, blockedDownY, groundedDown);

            const float normalHorizSq =
                normalResult.moved.x * normalResult.moved.x + normalResult.moved.z * normalResult.moved.z;
            const float stepMovedX = stepBox.min.x - box.min.x;
            const float stepMovedZ = stepBox.min.z - box.min.z;
            const float stepHorizSq = stepMovedX * stepMovedX + stepMovedZ * stepMovedZ;

            // Keep the stepped result only if it made more horizontal progress than the flat path
            if (stepHorizSq > normalHorizSq + 1e-6f) {
                MoveResult stepResult;
                stepResult.box = stepBox;
                stepResult.moved = stepBox.min - box.min;
                stepResult.blocked.x = blockedX;
                stepResult.blocked.z = blockedZ;
                stepResult.blocked.y = blockedDownY;
                stepResult.grounded = groundedDown;
                stepResult.stepped = true;
                return stepResult;
            }
        }

        return normalResult;
    }

    MoveResult moveAabb(const VoxelWorld& world, const Aabb& box, const glm::vec3& delta,
                        const MoveOptions& options) {
        return moveAabb(box, delta, solidQuery(world), options);
    }

} // namespace mge
