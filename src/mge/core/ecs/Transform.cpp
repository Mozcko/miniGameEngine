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
        // Maximum hierarchy depth before assuming a cycle
        constexpr int kMaxDepth = 256;

        struct Solver {
            Registry& registry;
            std::unordered_map<std::uint32_t, glm::mat4> done; // index -> world matrix for this frame

            glm::mat4 world(Entity entity, int depth) {
                auto it = done.find(entity.index);
                if (it != done.end()) return it->second;

                const Transform& transform = registry.get<Transform>(entity);
                glm::mat4 matrix = transform.localMatrix();

                const Parent* parent = registry.tryGet<Parent>(entity);
                if (parent && registry.has<Transform>(parent->entity)) {
                    if (depth >= kMaxDepth) {
                        MGE_LOG_WARN("ECS",
                                     "Transform hierarchy too deep or cyclic at entity ",
                                     entity.index, "; treating as root");
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
        // Collect entities first (adding WorldTransform during iteration is not allowed)
        std::vector<Entity> entities;
        registry.each<Transform>([&](Entity entity, Transform&) { entities.push_back(entity); });

        Solver solver{registry, {}};
        for (Entity entity : entities) {
            glm::mat4 matrix = solver.world(entity, 0);
            registry.add<WorldTransform>(entity, WorldTransform{matrix});
        }
    }
}
