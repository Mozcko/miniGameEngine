#include <mge/physics/VoxelCollision.h>

#include <mge/core/Log.h>

#include <algorithm>
#include <atomic>
#include <cmath>

namespace mge {

    namespace {
        constexpr float kTiny = 1e-5f;
        constexpr float kMaxDelta = 256.0f;

        // Comprueba si la caja ya interseca el volumen interior del bloque al iniciar el paso
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

        // Mueve la caja a lo largo de un único eje detectando colisiones contra bloques sólidos
        void moveSingleAxis(Aabb& currentBox, int axis, float deltaVal, const SolidQuery& isSolid, float skin,
                            bool& outBlocked, bool& outGrounded) {
            outBlocked = false;
            outGrounded = false;

            if (std::abs(deltaVal) < 1e-9f) {
                return;
            }

            // Volumen barrido a lo largo del eje actual
            glm::vec3 sweptMin = currentBox.min;
            glm::vec3 sweptMax = currentBox.max;
            if (deltaVal > 0.0f) {
                sweptMax[axis] += deltaVal;
            } else {
                sweptMin[axis] += deltaVal;
            }

            // Rango de bloques enteros que intersectan el volumen barrido
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
                            // Ignora bloques que la caja ya intersecaba antes de iniciar este eje
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

        // Ejecuta el movimiento secuencial normal en el orden Y, X, Z
        MoveResult moveSequence(const Aabb& initialBox, const glm::vec3& deltaVec, const SolidQuery& isSolid,
                                float skin) {
            MoveResult result;
            result.box = initialBox;

            // Orden estricto de resolución: Y, luego X, luego Z
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

        // Comprueba si la caja está apoyada en el suelo al inicio mediante una sonda de -skin * 2 en Y
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

        // Acotar cada componente del delta para prevenir escaneos excesivos ante deltas espurios
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

        // Movimiento normal Y -> X -> Z
        MoveResult normalResult = moveSequence(box, clampedDelta, isSolid, skin);

        // Escalón (step-up): solo si stepHeight > 0, delta.y <= 0, hubo bloqueo horizontal y estaba apoyado
        const bool horizontalBlocked = normalResult.blocked.x || normalResult.blocked.z;
        if (options.stepHeight > 0.0f && clampedDelta.y <= 0.0f && horizontalBlocked && groundedAtStart) {
            Aabb stepBox = box;
            bool blocked = false;
            bool grounded = false;

            // 1. Subir stepHeight
            moveSingleAxis(stepBox, 1, options.stepHeight, isSolid, skin, blocked, grounded);

            // 2. Mover X
            bool blockedX = false;
            moveSingleAxis(stepBox, 0, clampedDelta.x, isSolid, skin, blockedX, grounded);

            // 3. Mover Z
            bool blockedZ = false;
            moveSingleAxis(stepBox, 2, clampedDelta.z, isSolid, skin, blockedZ, grounded);

            // 4. Bajar (stepHeight + |delta.y|)
            bool blockedDownY = false;
            bool groundedDown = false;
            const float downDist = -(options.stepHeight + std::abs(clampedDelta.y));
            moveSingleAxis(stepBox, 1, downDist, isSolid, skin, blockedDownY, groundedDown);

            const float normalHorizSq =
                normalResult.moved.x * normalResult.moved.x + normalResult.moved.z * normalResult.moved.z;
            const float stepMovedX = stepBox.min.x - box.min.x;
            const float stepMovedZ = stepBox.min.z - box.min.z;
            const float stepHorizSq = stepMovedX * stepMovedX + stepMovedZ * stepMovedZ;

            // Conservar el resultado escalonado solo si avanzó más horizontalmente que el camino plano
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
