#pragma once

#include <mge/core/Time.h>
#include <mge/core/ecs/Entity.h>
#include <mge/core/ecs/Transform.h>
#include <mge/core/ecs/World.h>
#include <mge/core/math/Geometry.h>
#include <mge/physics/GridQuery.h>

#include <glm/glm.hpp>

namespace mge {

    enum class CharacterMode {
        Walk,
        Fly
    };

    // Intención de movimiento (la traduce el juego desde sus acciones; no lee teclas)
    struct CharacterInput {
        float moveForward = 0.0f; // -1..1
        float moveRight = 0.0f;   // -1..1
        float moveUp = 0.0f;      // -1..1, solo en Fly
        bool jump = false;        // Walk: salta si está apoyado (mantener = saltar al aterrizar)
        bool sprint = false;
    };

    // Componente ECS. Transform::position = pies del personaje (centro de la base de la caja).
    struct CharacterController {
        CharacterMode mode = CharacterMode::Walk;
        bool enabled = true; // false = no simula (ej: el chunk bajo los pies todavía no cargó)
        glm::vec3 halfExtents{0.3f, 0.9f, 0.3f};
        float eyeHeight = 1.62f; // Desde los pies
        float walkSpeed = 4.3f;
        float sprintMultiplier = 1.5f;
        float flySpeed = 10.0f;
        float flySprintMultiplier = 2.5f;
        float jumpSpeed = 8.0f; // ~1.25 bloques de salto con gravity 28
        float gravity = 28.0f;
        float maxFallSpeed = 60.0f;
        float stepHeight = 0.6f;    // Solo Walk y apoyado
        float acceleration = 40.0f; // Hacia la velocidad deseada en el plano XZ (suelo); en el aire usar 1/4
        float yaw = -90.0f;         // Grados; -90 = mira hacia -Z (igual que FlyController)
        float pitch = 0.0f;         // Grados, ±89
        CharacterInput input;       // Lo escribe el juego cada frame
        // --- Estado (lo escribe el sistema) ---
        glm::vec3 velocity{0.0f};
        bool grounded = false;
        glm::vec3 previousPosition{
            0.0f}; // Posición al empezar el último paso fijo (para interpolar la cámara)

        void look(float deltaYawDegrees, float deltaPitchDegrees); // Clampa pitch a ±89
        glm::vec3 forward() const;                                 // Dirección de la vista (con pitch)
        glm::vec3 flatForward() const; // Sin pitch, normalizada en XZ (para caminar)
        glm::vec3 right() const;
        void setMode(CharacterMode newMode); // Al pasar a Fly pone velocity.y = 0
        void toggleMode();
    };

    // Calcula la AABB que ocupa el personaje a partir de la posición de sus pies
    inline Aabb characterAabb(const glm::vec3& feet, const glm::vec3& halfExtents) {
        return Aabb(feet - glm::vec3(halfExtents.x, 0.0f, halfExtents.z),
                    feet + glm::vec3(halfExtents.x, 2.0f * halfExtents.y, halfExtents.z));
    }

    // Calcula la posición de los pies a partir de la AABB
    inline glm::vec3 characterFeet(const Aabb& box) {
        return glm::vec3((box.min.x + box.max.x) * 0.5f, box.min.y, (box.min.z + box.max.z) * 0.5f);
    }

    // Calcula la posición interpolada de los ojos del personaje
    inline glm::vec3 characterEyePosition(const CharacterController& controller, const Transform& transform,
                                          float alpha) {
        return glm::mix(controller.previousPosition, transform.position, alpha) +
               glm::vec3(0.0f, controller.eyeHeight, 0.0f);
    }

    // Un paso fijo para un personaje: pura, testeable sin ECS.
    glm::vec3 stepCharacter(CharacterController& controller, const glm::vec3& feet, float dt,
                            const SolidQuery& isSolid);
    void stepCharacter(CharacterController& controller, Transform& transform, float dt,
                       const SolidQuery& isSolid);

    // Registra el sistema en SystemPhase::Fixed: stepCharacter para cada entidad con <CharacterController,
    // Transform>.
    void addCharacterSystem(World& world, SolidQuery isSolid);

    // Componente para la entidad cámara: sigue a un personaje a la altura de los ojos.
    struct CharacterCamera {
        Entity target;
    };

    // Registra en SystemPhase::Render: para cada <CharacterCamera, Transform> cuyo target tenga
    // CharacterController y Transform, posición = mix(previousPosition, position, time.alpha()) + (0,
    // eyeHeight, 0) y rotación mirando a forward() (glm::quatLookAt(forward, up)). `time` debe vivir más que
    // el World.
    void addCharacterCameraSystem(World& world, const Time& time);

} // namespace mge
