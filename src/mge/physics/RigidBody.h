#pragma once

#include <mge/core/math/Geometry.h>

#include <glm/glm.hpp>

namespace mge {

    enum class ShapeType {
        Box,
        Sphere
    };

    // ECS component. Transform::position = body center. No rotation (bodies do not rotate in this
    // version).
    struct RigidBody {
        ShapeType shape = ShapeType::Box;
        glm::vec3 halfExtents{0.5f}; // Box
        float radius = 0.5f;         // Sphere
        float mass = 1.0f;           // 0 = static (immovable, infinite mass)
        float restitution = 0.2f;    // 0..1 bounce
        float friction = 0.5f;       // 0..1 (simplified Coulomb friction on tangential velocity)
        float gravityScale = 1.0f;
        float linearDamping = 0.01f;
        glm::vec3 velocity{0.0f};
        glm::vec3 previousPosition{0.0f}; // For render interpolation using alpha
        bool grounded = false;

        // AABB enclosing the shape around its center position
        Aabb bounds(const glm::vec3& center) const {
            if (shape == ShapeType::Sphere) {
                return Aabb(center - glm::vec3(radius), center + glm::vec3(radius));
            }
            return Aabb(center - halfExtents, center + halfExtents);
        }

        // Inverse mass (0 if mass <= 0, representing a static/infinite-mass body)
        float inverseMass() const { return mass <= 0.0f ? 0.0f : (1.0f / mass); }
    };

} // namespace mge
