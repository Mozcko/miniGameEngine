#include <mge/physics/CapsuleCharacterController.h>

#include <mge/core/Log.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <limits>
#include <utility>

#include <glm/gtc/quaternion.hpp>

namespace mge {

    namespace {

        constexpr float kEpsilon = 1e-6f;
        constexpr float kMaxDelta = 256.0f;
        constexpr int kMaxDepenetrationIters = 8;
        constexpr int kMaxSlideIters = 4;
        constexpr int kMaxSlidePlanes = 4;

        bool isFiniteVec3(const glm::vec3& v) {
            return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
        }

        // Depenetrates a capsule from overlapping triangles using MeshCollider::overlapCapsule (#203).
        void depenetrateCapsule(Capsule& cap, const MeshCollider& collider, float skin,
                                std::vector<std::uint32_t>& candidates) {
            std::vector<ShapeContact> contacts;
            for (int iter = 0; iter < kMaxDepenetrationIters; ++iter) {
                if (!collider.overlapCapsule(cap, contacts, 0.0f, candidates)) {
                    break;
                }
                ShapeContact deepest;
                for (const ShapeContact& c : contacts) {
                    if (c.penetration > deepest.penetration) {
                        deepest = c;
                    }
                }
                if (!deepest.hit || deepest.penetration <= 1e-5f) {
                    break;
                }
                cap.feet += deepest.normal * (deepest.penetration + skin);
            }
        }

        // Projects a horizontal displacement onto a walkable slope plane while preserving speed.
        glm::vec3 projectAlongWalkableSlope(const glm::vec3& remaining, const glm::vec3& slopeNormal) {
            const float origLen = glm::length(remaining);
            if (origLen <= kEpsilon || slopeNormal.y <= kEpsilon) {
                return glm::vec3(0.0f);
            }
            const float rise = -(slopeNormal.x * remaining.x + slopeNormal.z * remaining.z) / slopeNormal.y;
            const glm::vec3 tangent(remaining.x, rise, remaining.z);
            const float tanLen = glm::length(tangent);
            if (tanLen <= kEpsilon) {
                return glm::vec3(0.0f);
            }
            return tangent * (origLen / tanLen);
        }

        // Runs horizontal collide-and-slide with walkable slope climbing and steep-slope wall blocking.
        struct HorizontalSlideResult {
            glm::vec3 feet{0.0f};
            glm::vec3 groundNormal{0.0f, 1.0f, 0.0f};
            glm::vec3 steepNormal{0.0f, 1.0f, 0.0f};
            bool touchedWalkable = false;
            bool hitWall = false;
            bool onSteepSlope = false;
            bool hitCeiling = false;
        };

        HorizontalSlideResult slideHorizontal(const Capsule& startCap, const glm::vec3& horizDelta,
                                              const MeshCollider& collider, float minWalkableNormalY,
                                              float skin, std::vector<std::uint32_t>& candidates) {
            HorizontalSlideResult out;
            out.feet = startCap.feet;

            const float origLenSq = glm::dot(horizDelta, horizDelta);
            if (origLenSq < 1e-12f) {
                return out;
            }

            const glm::vec3 origDir = glm::normalize(horizDelta);
            Capsule cap = startCap;
            glm::vec3 remaining = horizDelta;
            std::array<glm::vec3, kMaxSlidePlanes> planes{};
            int planeCount = 0;

            for (int iter = 0; iter < kMaxSlideIters; ++iter) {
                if (glm::dot(remaining, remaining) < 1e-10f) {
                    break;
                }

                const ShapeCastHit hit = collider.capsuleCast(cap, remaining, skin, candidates);
                cap.feet += remaining * hit.fraction;
                if (!hit.hit) {
                    break;
                }

                remaining *= (1.0f - hit.fraction);
                glm::vec3 slideNormal = hit.normal;
                bool isWalkable = false;

                if (hit.normal.y >= minWalkableNormalY) {
                    isWalkable = true;
                    out.touchedWalkable = true;
                    out.groundNormal = hit.normal;
                } else if (hit.normal.y <= -0.5f) {
                    out.hitCeiling = true;
                } else {
                    out.hitWall = true;
                    if (hit.normal.y > 0.05f) {
                        out.onSteepSlope = true;
                        out.steepNormal = hit.normal;
                    }
                    // Treat steep slopes as vertical walls during horizontal motion so the character
                    // cannot climb slopes steeper than maxSlopeAngle.
                    const glm::vec3 flatNormal(hit.normal.x, 0.0f, hit.normal.z);
                    const float flatLenSq = glm::dot(flatNormal, flatNormal);
                    if (flatLenSq > 1e-8f) {
                        slideNormal = flatNormal / std::sqrt(flatLenSq);
                    }
                }

                // Record unique slide plane
                bool duplicate = false;
                for (int p = 0; p < planeCount; ++p) {
                    if (glm::dot(planes[static_cast<std::size_t>(p)], slideNormal) > 0.999f) {
                        duplicate = true;
                        break;
                    }
                }
                if (!duplicate && planeCount < kMaxSlidePlanes) {
                    planes[static_cast<std::size_t>(planeCount++)] = slideNormal;
                }

                if (planeCount == 1) {
                    if (isWalkable) {
                        remaining = projectAlongWalkableSlope(remaining, slideNormal);
                    } else {
                        remaining -= glm::dot(remaining, slideNormal) * slideNormal;
                    }
                } else if (planeCount == 2) {
                    const glm::vec3 crease = glm::cross(planes[0], planes[1]);
                    const float creaseLenSq = glm::dot(crease, crease);
                    if (creaseLenSq > 1e-8f) {
                        const glm::vec3 creaseDir = crease / std::sqrt(creaseLenSq);
                        remaining = creaseDir * glm::dot(remaining, creaseDir);
                    } else {
                        remaining = glm::vec3(0.0f);
                    }
                } else {
                    remaining = glm::vec3(0.0f);
                }

                // Never slide backwards relative to the original horizontal direction
                if (glm::dot(remaining, origDir) <= 0.0f) {
                    break;
                }
            }

            out.feet = cap.feet;
            return out;
        }

        // Full 3D collide-and-slide (used in Fly mode).
        CapsuleMoveResult moveCapsuleFly(const Capsule& startCap, const glm::vec3& delta,
                                         const MeshCollider& collider, float skin,
                                         std::vector<std::uint32_t>& candidates) {
            CapsuleMoveResult res;
            Capsule cap = startCap;
            depenetrateCapsule(cap, collider, skin, candidates);

            glm::vec3 remaining = delta;
            std::array<glm::vec3, kMaxSlidePlanes> planes{};
            int planeCount = 0;

            for (int iter = 0; iter < kMaxSlideIters; ++iter) {
                if (glm::dot(remaining, remaining) < 1e-10f) {
                    break;
                }
                const ShapeCastHit hit = collider.capsuleCast(cap, remaining, skin, candidates);
                cap.feet += remaining * hit.fraction;
                if (!hit.hit) {
                    break;
                }

                if (hit.normal.y > 0.5f) {
                    res.grounded = true;
                    res.groundNormal = hit.normal;
                } else if (hit.normal.y < -0.5f) {
                    res.hitCeiling = true;
                } else {
                    res.hitWall = true;
                }

                remaining *= (1.0f - hit.fraction);
                if (planeCount < kMaxSlidePlanes) {
                    planes[static_cast<std::size_t>(planeCount++)] = hit.normal;
                }

                if (planeCount == 1) {
                    remaining -= glm::dot(remaining, hit.normal) * hit.normal;
                } else if (planeCount == 2) {
                    const glm::vec3 crease = glm::cross(planes[0], planes[1]);
                    const float creaseLenSq = glm::dot(crease, crease);
                    if (creaseLenSq > 1e-8f) {
                        const glm::vec3 creaseDir = crease / std::sqrt(creaseLenSq);
                        remaining = creaseDir * glm::dot(remaining, creaseDir);
                    } else {
                        remaining = glm::vec3(0.0f);
                    }
                } else {
                    remaining = glm::vec3(0.0f);
                }
            }

            res.feet = cap.feet;
            res.moved = res.feet - startCap.feet;
            return res;
        }

    } // namespace

    // --- moveCapsule ---

    CapsuleMoveResult moveCapsule(const Capsule& capsule, const glm::vec3& delta,
                                  const MeshCollider& collider, const CapsuleMoveOptions& options) {
        CapsuleMoveResult result;
        result.feet = capsule.feet;

        if (!isFiniteVec3(capsule.feet) || !isFiniteVec3(delta) || !std::isfinite(capsule.radius) ||
            !std::isfinite(capsule.height) || capsule.radius <= 0.0f) {
            return result;
        }

        const float skin = (std::isnan(options.skin) || options.skin < 0.0f) ? 1e-3f : options.skin;

        glm::vec3 clampedDelta = delta;
        for (int axis = 0; axis < 3; ++axis) {
            if (std::abs(clampedDelta[axis]) > kMaxDelta) {
                static std::atomic<bool> warnedClamp{false};
                if (!warnedClamp.exchange(true, std::memory_order_relaxed)) {
                    MGE_LOG_WARN("Physics", "moveCapsule: delta clamped to +-256 units");
                }
                clampedDelta[axis] = std::clamp(clampedDelta[axis], -kMaxDelta, kMaxDelta);
            }
        }

        if (collider.empty()) {
            result.feet = capsule.feet + clampedDelta;
            result.moved = clampedDelta;
            return result;
        }

        std::vector<std::uint32_t> candidates;
        candidates.reserve(32);

        if (options.flyMode) {
            return moveCapsuleFly(capsule, clampedDelta, collider, skin, candidates);
        }

        const float clampedSlopeDeg = std::clamp(options.maxSlopeAngle, 0.0f, 89.0f);
        const float minWalkableNormalY = std::cos(glm::radians(clampedSlopeDeg));

        Capsule cap = capsule;
        depenetrateCapsule(cap, collider, skin, candidates);

        // 1) Horizontal collide-and-slide
        const glm::vec3 horizDelta(clampedDelta.x, 0.0f, clampedDelta.z);
        const HorizontalSlideResult normalHoriz =
            slideHorizontal(cap, horizDelta, collider, minWalkableNormalY, skin, candidates);

        glm::vec3 chosenFeet = normalHoriz.feet;
        bool steppedThisMove = false;
        bool steppedGrounded = false;
        glm::vec3 steppedGroundNormal(0.0f, 1.0f, 0.0f);

        result.hitWall = normalHoriz.hitWall;
        result.hitCeiling = normalHoriz.hitCeiling;
        if (normalHoriz.onSteepSlope) {
            result.onSteepSlope = true;
            result.slideNormal = normalHoriz.steepNormal;
        }

        // 2) Step-up attempt if grounded, moving horizontally, and blocked by an obstacle
        const float stepHeight = std::max(options.stepHeight, 0.0f);
        if (options.allowStepping && options.wasGrounded && stepHeight > 0.0f && clampedDelta.y <= 0.0f &&
            normalHoriz.hitWall && glm::dot(horizDelta, horizDelta) > 1e-10f) {
            // 2a. Sweep up by stepHeight
            const ShapeCastHit upHit =
                collider.capsuleCast(cap, glm::vec3(0.0f, stepHeight, 0.0f), skin, candidates);
            const float actualUp = stepHeight * upHit.fraction;

            if (actualUp > 1e-3f) {
                Capsule raisedCap = cap;
                raisedCap.feet.y += actualUp;

                // 2b. Horizontal slide from raised position
                const HorizontalSlideResult stepHoriz =
                    slideHorizontal(raisedCap, horizDelta, collider, minWalkableNormalY, skin, candidates);

                // 2c. Sweep down to land on the step surface
                Capsule downCap = raisedCap;
                downCap.feet = stepHoriz.feet;
                const float downDist = actualUp + std::abs(clampedDelta.y) + skin * 2.0f;
                const ShapeCastHit downHit =
                    collider.capsuleCast(downCap, glm::vec3(0.0f, -downDist, 0.0f), skin, candidates);

                const bool landingWalkable = downHit.hit && (downHit.normal.y >= minWalkableNormalY ||
                                                             downHit.faceNormal.y >= minWalkableNormalY);

                if (landingWalkable) {
                    const glm::vec3 candidateFeet =
                        downCap.feet + glm::vec3(0.0f, -downDist * downHit.fraction, 0.0f);
                    const float feetRise = candidateFeet.y - cap.feet.y;
                    const float surfaceRise = downHit.point.y - cap.feet.y;

                    const glm::vec2 normalHorizMoved(normalHoriz.feet.x - cap.feet.x,
                                                     normalHoriz.feet.z - cap.feet.z);
                    const glm::vec2 stepHorizMoved(candidateFeet.x - cap.feet.x,
                                                   candidateFeet.z - cap.feet.z);

                    if (feetRise <= stepHeight + 1e-3f && surfaceRise <= stepHeight + 1e-3f &&
                        glm::dot(stepHorizMoved, stepHorizMoved) >
                            glm::dot(normalHorizMoved, normalHorizMoved) + 1e-5f) {
                        chosenFeet = candidateFeet;
                        steppedThisMove = true;
                        steppedGrounded = true;
                        steppedGroundNormal =
                            (downHit.normal.y >= minWalkableNormalY) ? downHit.normal : downHit.faceNormal;
                        result.hitWall = stepHoriz.hitWall;
                        result.onSteepSlope = false;
                    }
                }
            }
        }

        cap.feet = chosenFeet;
        if (steppedThisMove) {
            result.stepped = true;
            result.grounded = steppedGrounded;
            result.groundNormal = steppedGroundNormal;
        }

        // 3) Vertical movement (skipped downward if step-up already landed cleanly on the step)
        if (!steppedThisMove && std::abs(clampedDelta.y) > 1e-9f) {
            glm::vec3 vertRem(0.0f, clampedDelta.y, 0.0f);
            for (int iter = 0; iter < kMaxSlideIters; ++iter) {
                if (glm::dot(vertRem, vertRem) < 1e-10f) {
                    break;
                }
                const ShapeCastHit vertHit = collider.capsuleCast(cap, vertRem, skin, candidates);
                cap.feet += vertRem * vertHit.fraction;
                if (!vertHit.hit) {
                    break;
                }

                if (clampedDelta.y < 0.0f) {
                    if (vertHit.normal.y >= minWalkableNormalY) {
                        // Landed on walkable ground: stop without sliding downhill
                        result.grounded = true;
                        result.groundNormal = vertHit.normal;
                        result.onSteepSlope = false;
                        break;
                    }
                    // Hit a steep slope while falling: slide down it
                    result.onSteepSlope = true;
                    result.slideNormal = vertHit.normal;
                    vertRem *= (1.0f - vertHit.fraction);
                    vertRem -= glm::dot(vertRem, vertHit.normal) * vertHit.normal;
                } else {
                    // Moving upward: hit ceiling or overhang
                    if (vertHit.normal.y <= -0.1f) {
                        result.hitCeiling = true;
                    }
                    break;
                }
            }
        }

        // 4) Ground probe / ground snapping when walking off downhill slopes or small drops
        if (!result.grounded && clampedDelta.y <= 0.0f) {
            const float probeDist =
                (options.allowGroundSnap && options.wasGrounded && options.groundSnapDistance > 0.0f)
                    ? options.groundSnapDistance
                    : (skin * 2.0f);

            const ShapeCastHit snapHit =
                collider.capsuleCast(cap, glm::vec3(0.0f, -probeDist, 0.0f), skin, candidates);
            if (snapHit.hit && snapHit.normal.y >= minWalkableNormalY) {
                const float drop = probeDist * snapHit.fraction;
                cap.feet.y -= drop;
                result.grounded = true;
                result.groundNormal = snapHit.normal;
                result.onSteepSlope = false;
                if (options.allowGroundSnap && options.wasGrounded && drop > skin * 2.0f) {
                    result.snapped = true;
                }
            } else if (snapHit.hit && snapHit.normal.y > 0.05f && snapHit.normal.y < minWalkableNormalY) {
                result.onSteepSlope = true;
                result.slideNormal = snapHit.normal;
            }
        }

        result.feet = cap.feet;
        result.moved = result.feet - capsule.feet;
        return result;
    }

    // --- CapsuleCharacterController ---

    void CapsuleCharacterController::look(float deltaYawDegrees, float deltaPitchDegrees) {
        yaw += deltaYawDegrees;
        pitch = std::clamp(pitch + deltaPitchDegrees, -89.0f, 89.0f);
    }

    glm::vec3 CapsuleCharacterController::forward() const {
        const float yawRad = glm::radians(yaw);
        const float pitchRad = glm::radians(pitch);
        return glm::normalize(glm::vec3(std::cos(yawRad) * std::cos(pitchRad), std::sin(pitchRad),
                                        std::sin(yawRad) * std::cos(pitchRad)));
    }

    glm::vec3 CapsuleCharacterController::flatForward() const {
        const float yawRad = glm::radians(yaw);
        return glm::normalize(glm::vec3(std::cos(yawRad), 0.0f, std::sin(yawRad)));
    }

    glm::vec3 CapsuleCharacterController::right() const {
        return glm::normalize(glm::cross(forward(), glm::vec3(0.0f, 1.0f, 0.0f)));
    }

    void CapsuleCharacterController::setMode(CharacterMode newMode) {
        mode = newMode;
        if (mode == CharacterMode::Fly) {
            velocity.y = 0.0f;
        }
    }

    void CapsuleCharacterController::toggleMode() {
        setMode(mode == CharacterMode::Walk ? CharacterMode::Fly : CharacterMode::Walk);
    }

    glm::vec3 stepCapsuleCharacter(CapsuleCharacterController& controller, const glm::vec3& feet, float dt,
                                   const MeshCollider& collider) {
        controller.previousPosition = feet;
        if (dt <= 0.0f || !controller.enabled) {
            return feet;
        }

        if (controller.mode == CharacterMode::Walk) {
            // Desired horizontal velocity on the XZ plane
            glm::vec3 wishDir = controller.flatForward() * controller.input.moveForward +
                                controller.right() * controller.input.moveRight;
            if (glm::dot(wishDir, wishDir) > kEpsilon) {
                wishDir = glm::normalize(wishDir);
            } else {
                wishDir = glm::vec3(0.0f);
            }

            const float speed =
                controller.walkSpeed * (controller.input.sprint ? controller.sprintMultiplier : 1.0f);
            const glm::vec3 wishVel = wishDir * speed;

            // Acceleration towards wishVel (reduced to 1/4 in air)
            const float accelRate =
                controller.grounded ? controller.acceleration : (controller.acceleration * 0.25f);
            const float maxAccel = accelRate * dt;

            glm::vec2 currHoriz(controller.velocity.x, controller.velocity.z);
            const glm::vec2 targetHoriz(wishVel.x, wishVel.z);
            const glm::vec2 diff = targetHoriz - currHoriz;
            const float diffLen = glm::length(diff);
            if (diffLen <= maxAccel || diffLen < kEpsilon) {
                currHoriz = targetHoriz;
            } else {
                currHoriz += (diff / diffLen) * maxAccel;
            }
            controller.velocity.x = currHoriz.x;
            controller.velocity.z = currHoriz.y;

            const bool wasGrounded = controller.grounded;
            bool jumped = false;
            if (controller.grounded && controller.input.jump) {
                controller.velocity.y = controller.jumpSpeed;
                controller.grounded = false;
                jumped = true;
            } else {
                controller.velocity.y -= controller.gravity * dt;
                if (controller.velocity.y < -controller.maxFallSpeed) {
                    controller.velocity.y = -controller.maxFallSpeed;
                }
            }

            CapsuleMoveOptions moveOpts;
            moveOpts.maxSlopeAngle = controller.maxSlopeAngle;
            moveOpts.stepHeight = controller.stepHeight;
            moveOpts.groundSnapDistance = controller.groundSnapDistance;
            moveOpts.skin = controller.skin;
            moveOpts.wasGrounded = wasGrounded && !jumped;
            moveOpts.allowStepping = wasGrounded && !jumped;
            moveOpts.allowGroundSnap = wasGrounded && !jumped;
            moveOpts.flyMode = false;

            const CapsuleMoveResult res =
                moveCapsule(controller.capsule(feet), controller.velocity * dt, collider, moveOpts);

            if (res.grounded) {
                controller.velocity.y = 0.0f;
            } else if (res.hitCeiling && controller.velocity.y > 0.0f) {
                controller.velocity.y = 0.0f;
            } else if (res.onSteepSlope) {
                const float vn = glm::dot(controller.velocity, res.slideNormal);
                if (vn < 0.0f) {
                    controller.velocity -= res.slideNormal * vn;
                }
            }

            controller.grounded = res.grounded;
            controller.groundNormal = res.grounded ? res.groundNormal : glm::vec3(0.0f, 1.0f, 0.0f);
            controller.onSteepSlope = res.onSteepSlope;
            controller.stepped = res.stepped;
            controller.snapped = res.snapped;
            return res.feet;
        }

        // CharacterMode::Fly
        glm::vec3 moveDir = controller.forward() * controller.input.moveForward +
                            controller.right() * controller.input.moveRight +
                            glm::vec3(0.0f, 1.0f, 0.0f) * controller.input.moveUp;
        if (glm::dot(moveDir, moveDir) > kEpsilon) {
            moveDir = glm::normalize(moveDir);
        } else {
            moveDir = glm::vec3(0.0f);
        }

        const float speed =
            controller.flySpeed * (controller.input.sprint ? controller.flySprintMultiplier : 1.0f);
        controller.velocity = moveDir * speed;

        CapsuleMoveOptions moveOpts;
        moveOpts.skin = controller.skin;
        moveOpts.flyMode = true;

        const CapsuleMoveResult res =
            moveCapsule(controller.capsule(feet), controller.velocity * dt, collider, moveOpts);

        if (res.hitCeiling && controller.velocity.y > 0.0f) {
            controller.velocity.y = 0.0f;
        }
        if (res.grounded && controller.velocity.y < 0.0f) {
            controller.velocity.y = 0.0f;
        }
        controller.grounded = res.grounded;
        controller.groundNormal = res.grounded ? res.groundNormal : glm::vec3(0.0f, 1.0f, 0.0f);
        controller.onSteepSlope = false;
        controller.stepped = false;
        controller.snapped = false;
        return res.feet;
    }

    void stepCapsuleCharacter(CapsuleCharacterController& controller, Transform& transform, float dt,
                              const MeshCollider& collider) {
        transform.position = stepCapsuleCharacter(controller, transform.position, dt, collider);
    }

    void addCapsuleCharacterSystem(World& world, const MeshCollider& collider) {
        world.addSystem(SystemPhase::Fixed, "CapsuleCharacterSystem", [&collider](World& w, float dt) {
            w.registry().each<CapsuleCharacterController, Transform>(
                [dt, &collider](Entity /*e*/, CapsuleCharacterController& controller, Transform& transform) {
                    stepCapsuleCharacter(controller, transform, dt, collider);
                });
        });
    }

    void addCapsuleCharacterCameraSystem(World& world, const Time& time) {
        world.addSystem(SystemPhase::Render, "CapsuleCharacterCameraSystem", [&time](World& w, float /*dt*/) {
            auto& reg = w.registry();
            reg.each<CapsuleCharacterCamera, Transform>(
                [&reg, &time](Entity /*e*/, const CapsuleCharacterCamera& cam, Transform& camTransform) {
                    if (!reg.valid(cam.target)) {
                        return;
                    }
                    const auto* controller = reg.tryGet<CapsuleCharacterController>(cam.target);
                    const auto* targetTransform = reg.tryGet<Transform>(cam.target);
                    if (!controller || !targetTransform) {
                        return;
                    }
                    camTransform.position =
                        capsuleCharacterEyePosition(*controller, *targetTransform, time.alpha());
                    camTransform.rotation =
                        glm::quatLookAt(controller->forward(), glm::vec3(0.0f, 1.0f, 0.0f));
                });
        });
    }

} // namespace mge
