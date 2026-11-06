#include <mge/physics/CharacterController.h>
#include <mge/physics/VoxelCollision.h>

#include <algorithm>
#include <cmath>

#include <glm/gtc/quaternion.hpp>

namespace mge {

    void CharacterController::look(float deltaYawDegrees, float deltaPitchDegrees) {
        yaw += deltaYawDegrees;
        pitch = std::clamp(pitch + deltaPitchDegrees, -89.0f, 89.0f);
    }

    glm::vec3 CharacterController::forward() const {
        const float yawRad = glm::radians(yaw);
        const float pitchRad = glm::radians(pitch);
        return glm::normalize(glm::vec3(std::cos(yawRad) * std::cos(pitchRad), std::sin(pitchRad),
                                        std::sin(yawRad) * std::cos(pitchRad)));
    }

    glm::vec3 CharacterController::flatForward() const {
        const float yawRad = glm::radians(yaw);
        return glm::normalize(glm::vec3(std::cos(yawRad), 0.0f, std::sin(yawRad)));
    }

    glm::vec3 CharacterController::right() const {
        return glm::normalize(glm::cross(forward(), glm::vec3(0.0f, 1.0f, 0.0f)));
    }

    void CharacterController::setMode(CharacterMode newMode) {
        mode = newMode;
        if (mode == CharacterMode::Fly) {
            velocity.y = 0.0f;
        }
    }

    void CharacterController::toggleMode() {
        setMode(mode == CharacterMode::Walk ? CharacterMode::Fly : CharacterMode::Walk);
    }

    glm::vec3 stepCharacter(CharacterController& controller, const glm::vec3& feet, float dt,
                            const SolidQuery& isSolid) {
        if (dt <= 0.0f || !controller.enabled) {
            return feet;
        }
        controller.previousPosition = feet;

        if (controller.mode == CharacterMode::Walk) {
            // Velocidad horizontal deseada en el plano XZ
            glm::vec3 wishDir = controller.flatForward() * controller.input.moveForward +
                                controller.right() * controller.input.moveRight;
            if (glm::dot(wishDir, wishDir) > 1e-6f) {
                wishDir = glm::normalize(wishDir);
            } else {
                wishDir = glm::vec3(0.0f);
            }

            const float speed =
                controller.walkSpeed * (controller.input.sprint ? controller.sprintMultiplier : 1.0f);
            const glm::vec3 wishVel = wishDir * speed;

            // Aceleración hacia wishVel (en el aire se reduce a 1/4)
            const float accelRate =
                controller.grounded ? controller.acceleration : (controller.acceleration * 0.25f);
            const float maxAccel = accelRate * dt;

            glm::vec2 currHoriz(controller.velocity.x, controller.velocity.z);
            const glm::vec2 targetHoriz(wishVel.x, wishVel.z);
            const glm::vec2 diff = targetHoriz - currHoriz;
            const float diffLen = glm::length(diff);
            if (diffLen <= maxAccel || diffLen < 1e-6f) {
                currHoriz = targetHoriz;
            } else {
                currHoriz += (diff / diffLen) * maxAccel;
            }
            controller.velocity.x = currHoriz.x;
            controller.velocity.z = currHoriz.y;

            // Salto si está apoyado e input.jump está activo; si no, aplicar gravedad
            const bool wasGrounded = controller.grounded;
            if (controller.grounded && controller.input.jump) {
                controller.velocity.y = controller.jumpSpeed;
                controller.grounded = false;
            } else {
                controller.velocity.y -= controller.gravity * dt;
                if (controller.velocity.y < -controller.maxFallSpeed) {
                    controller.velocity.y = -controller.maxFallSpeed;
                }
            }

            // Mover caja con colisiones
            const Aabb startBox = characterAabb(feet, controller.halfExtents);
            MoveOptions moveOpts;
            moveOpts.stepHeight = wasGrounded ? controller.stepHeight : 0.0f;
            moveOpts.skin = 1e-4f;

            const MoveResult res = moveAabb(startBox, controller.velocity * dt, isSolid, moveOpts);

            if (res.blocked.y) {
                controller.velocity.y = 0.0f;
            }
            if (res.blocked.x) {
                controller.velocity.x = 0.0f;
            }
            if (res.blocked.z) {
                controller.velocity.z = 0.0f;
            }
            controller.grounded = res.grounded;
            return characterFeet(res.box);

        } else { // CharacterMode::Fly
            glm::vec3 moveDir = controller.forward() * controller.input.moveForward +
                                controller.right() * controller.input.moveRight +
                                glm::vec3(0.0f, 1.0f, 0.0f) * controller.input.moveUp;
            if (glm::dot(moveDir, moveDir) > 1e-6f) {
                moveDir = glm::normalize(moveDir);
            } else {
                moveDir = glm::vec3(0.0f);
            }

            const float speed =
                controller.flySpeed * (controller.input.sprint ? controller.flySprintMultiplier : 1.0f);
            controller.velocity = moveDir * speed;

            const Aabb startBox = characterAabb(feet, controller.halfExtents);
            MoveOptions moveOpts;
            moveOpts.stepHeight = 0.0f;
            moveOpts.skin = 1e-4f;

            const MoveResult res = moveAabb(startBox, controller.velocity * dt, isSolid, moveOpts);

            if (res.blocked.y) {
                controller.velocity.y = 0.0f;
            }
            if (res.blocked.x) {
                controller.velocity.x = 0.0f;
            }
            if (res.blocked.z) {
                controller.velocity.z = 0.0f;
            }
            controller.grounded = res.grounded;
            return characterFeet(res.box);
        }
    }

    void stepCharacter(CharacterController& controller, Transform& transform, float dt,
                       const SolidQuery& isSolid) {
        transform.position = stepCharacter(controller, transform.position, dt, isSolid);
    }

    void addCharacterSystem(World& world, SolidQuery isSolid) {
        world.addSystem(
            SystemPhase::Fixed, "CharacterSystem", [solidFn = std::move(isSolid)](World& w, float dt) {
                w.registry().each<CharacterController, Transform>(
                    [dt, &solidFn](Entity /*e*/, CharacterController& controller, Transform& transform) {
                        stepCharacter(controller, transform, dt, solidFn);
                    });
            });
    }

    void addCharacterCameraSystem(World& world, const Time& time) {
        world.addSystem(SystemPhase::Render, "CharacterCameraSystem", [&time](World& w, float /*dt*/) {
            auto& reg = w.registry();
            reg.each<CharacterCamera, Transform>([&reg, &time](Entity /*e*/, const CharacterCamera& cam,
                                                               Transform& camTransform) {
                if (!reg.valid(cam.target)) {
                    return;
                }
                const auto* controller = reg.tryGet<CharacterController>(cam.target);
                const auto* targetTransform = reg.tryGet<Transform>(cam.target);
                if (!controller || !targetTransform) {
                    return;
                }
                camTransform.position = characterEyePosition(*controller, *targetTransform, time.alpha());
                camTransform.rotation = glm::quatLookAt(controller->forward(), glm::vec3(0.0f, 1.0f, 0.0f));
            });
        });
    }

} // namespace mge
