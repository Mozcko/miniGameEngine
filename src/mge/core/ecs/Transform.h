#pragma once

#include <mge/core/ecs/Entity.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace mge {

    class Registry;

    // Local position/rotation/scale (relative to parent if the entity has a Parent component)
    struct Transform {
        glm::vec3 position{0.0f};
        glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f}; // Identity (w, x, y, z)
        glm::vec3 scale{1.0f};

        // Local matrix: T * R * S
        glm::mat4 localMatrix() const;
    };

    // Hierarchy: the entity is a child of parent (which must also have a Transform)
    struct Parent {
        Entity entity;
    };

    // World matrix computed by TransformSystem (do not write manually)
    struct WorldTransform {
        glm::mat4 matrix{1.0f};
    };

    // Computes WorldTransform for every entity with Transform: parents before children
    // (world = world(parent) * local). Adds WorldTransform if missing. Invalid parents or parents
    // without Transform are treated as root; cycles are broken with a warning in the log.
    class TransformSystem {
    public:
        static void update(Registry& registry);
    };
}
