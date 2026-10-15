#include <mge/core/ecs/Transform.h>

#include <mge/core/Log.h>
#include <mge/core/ecs/Registry.h>

#include <glm/gtc/matrix_transform.hpp>
#include <unordered_map>
#include <vector>

namespace mge {

    glm::mat4 Transform::localMatrix() const {
        glm::mat4 matrix = glm::translate(glm::mat4(1.0f), position);
        matrix *= glm::mat4_cast(rotation);
        return glm::scale(matrix, scale);
    }

    namespace {
        // Profundidad máxima de la jerarquía antes de asumir un ciclo
        constexpr int kMaxDepth = 256;

        struct Solver {
            Registry& registry;
            std::unordered_map<std::uint32_t, glm::mat4> done; // index -> matriz de mundo del frame

            glm::mat4 world(Entity entity, int depth) {
                auto it = done.find(entity.index);
                if (it != done.end()) return it->second;

                const Transform& transform = registry.get<Transform>(entity);
                glm::mat4 matrix = transform.localMatrix();

                const Parent* parent = registry.tryGet<Parent>(entity);
                if (parent && registry.has<Transform>(parent->entity)) {
                    if (depth >= kMaxDepth) {
                        MGE_LOG_WARN("ECS",
                                     "Jerarquía de Transform demasiado profunda o con ciclo en la entidad ",
                                     entity.index, "; se trata como raíz");
                    } else {
                        matrix = world(parent->entity, depth + 1) * matrix;
                    }
                }
                done.emplace(entity.index, matrix);
                return matrix;
            }
        };
    }

    void TransformSystem::update(Registry& registry) {
        // Primero juntar las entidades (agregar WorldTransform durante la iteración no está permitido)
        std::vector<Entity> entities;
        registry.each<Transform>([&](Entity entity, Transform&) { entities.push_back(entity); });

        Solver solver{registry, {}};
        for (Entity entity : entities) {
            glm::mat4 matrix = solver.world(entity, 0);
            registry.add<WorldTransform>(entity, WorldTransform{matrix});
        }
    }
}
