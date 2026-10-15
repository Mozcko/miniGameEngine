#pragma once

#include <mge/core/ecs/Entity.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace mge {

    class Registry;

    // Posición/rotación/escala local (relativa al padre si la entidad tiene Parent)
    struct Transform {
        glm::vec3 position{0.0f};
        glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f}; // Identidad (w, x, y, z)
        glm::vec3 scale{1.0f};

        // Matriz local: T * R * S
        glm::mat4 localMatrix() const;
    };

    // Jerarquía: la entidad cuelga de parent (que también debe tener Transform)
    struct Parent {
        Entity entity;
    };

    // Matriz de mundo calculada por TransformSystem (no escribir a mano)
    struct WorldTransform {
        glm::mat4 matrix{1.0f};
    };

    // Calcula WorldTransform para toda entidad con Transform: padres antes que hijos
    // (world = world(padre) * local). Agrega WorldTransform si falta. Padres inválidos o sin
    // Transform se tratan como raíz; los ciclos se cortan con un aviso en el log.
    class TransformSystem {
    public:
        static void update(Registry& registry);
    };
}
