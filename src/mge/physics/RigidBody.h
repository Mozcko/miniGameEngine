#pragma once

#include <mge/core/math/Geometry.h>

#include <glm/glm.hpp>

namespace mge {

    enum class ShapeType {
        Box,
        Sphere
    };

    // Componente ECS. Transform::position = centro del cuerpo. Sin rotación (los cuerpos no giran en esta
    // versión).
    struct RigidBody {
        ShapeType shape = ShapeType::Box;
        glm::vec3 halfExtents{0.5f}; // Box
        float radius = 0.5f;         // Sphere
        float mass = 1.0f;           // 0 = estático (no se mueve, masa infinita)
        float restitution = 0.2f;    // 0..1 rebote
        float friction = 0.5f;       // 0..1 (Coulomb simplificado sobre la velocidad tangencial)
        float gravityScale = 1.0f;
        float linearDamping = 0.01f;
        glm::vec3 velocity{0.0f};
        glm::vec3 previousPosition{0.0f}; // Para interpolar el render con alpha
        bool grounded = false;

        // Caja AABB que envuelve la forma según su posición central
        Aabb bounds(const glm::vec3& center) const {
            if (shape == ShapeType::Sphere) {
                return Aabb(center - glm::vec3(radius), center + glm::vec3(radius));
            }
            return Aabb(center - halfExtents, center + halfExtents);
        }

        // Masa inversa (0 si mass <= 0, simulando cuerpo estático/infinito)
        float inverseMass() const { return mass <= 0.0f ? 0.0f : (1.0f / mass); }
    };

} // namespace mge
