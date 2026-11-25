#include <mge/gltf/GltfModel.h>

#include <mge/core/Log.h>

#include <cstddef>
#include <vector>

namespace mge {

    namespace {
        constexpr int kMaxNodeDepth = 256;

        void traverseNode(const std::vector<GltfNode>& nodes, int nodeIndex, const glm::mat4& parentWorld,
                          int depth, std::vector<glm::mat4>& worldMatrices, std::vector<bool>& visited) {
            if (nodeIndex < 0 || static_cast<std::size_t>(nodeIndex) >= nodes.size()) return;
            const std::size_t idx = static_cast<std::size_t>(nodeIndex);
            if (depth >= kMaxNodeDepth) {
                MGE_LOG_WARN("Gltf", "Node hierarchy too deep or cyclic at node ", nodeIndex,
                             "; stopping traversal");
                return;
            }
            if (visited[idx]) return;
            visited[idx] = true;

            const glm::mat4 world = parentWorld * nodes[idx].transform.localMatrix();
            worldMatrices[idx] = world;

            for (int childIndex : nodes[idx].children) {
                traverseNode(nodes, childIndex, world, depth + 1, worldMatrices, visited);
            }
        }
    }

    std::vector<glm::mat4> GltfModel::computeWorldTransforms() const {
        const std::size_t count = nodes.size();
        std::vector<glm::mat4> worldMatrices(count, glm::mat4(1.0f));
        if (count == 0) return worldMatrices;

        std::vector<bool> visited(count, false);

        if (!scenes.empty()) {
            const int sceneIdx =
                (defaultScene >= 0 && static_cast<std::size_t>(defaultScene) < scenes.size()) ? defaultScene
                                                                                              : 0;
            for (int rootIndex : scenes[static_cast<std::size_t>(sceneIdx)].rootNodes) {
                traverseNode(nodes, rootIndex, glm::mat4(1.0f), 0, worldMatrices, visited);
            }
        }

        // Compute transforms for any remaining unvisited nodes (disconnected or no scenes defined)
        std::vector<bool> hasParent(count, false);
        for (const GltfNode& node : nodes) {
            for (int childIndex : node.children) {
                if (childIndex >= 0 && static_cast<std::size_t>(childIndex) < count) {
                    hasParent[static_cast<std::size_t>(childIndex)] = true;
                }
            }
        }
        for (std::size_t i = 0; i < count; i++) {
            if (!visited[i] && !hasParent[i]) {
                traverseNode(nodes, static_cast<int>(i), glm::mat4(1.0f), 0, worldMatrices, visited);
            }
        }
        // Fallback for pure cycles with no root
        for (std::size_t i = 0; i < count; i++) {
            if (!visited[i]) {
                traverseNode(nodes, static_cast<int>(i), glm::mat4(1.0f), 0, worldMatrices, visited);
            }
        }

        return worldMatrices;
    }

    Aabb GltfModel::bounds() const {
        Aabb box;
        if (nodes.empty()) {
            for (const GltfMesh& mesh : meshes) {
                for (const GltfPrimitive& prim : mesh.primitives) {
                    box.merge(prim.mesh.bounds());
                }
            }
            return box;
        }

        const std::vector<glm::mat4> worldMatrices = computeWorldTransforms();
        for (std::size_t i = 0; i < nodes.size(); i++) {
            const int meshIndex = nodes[i].meshIndex;
            if (meshIndex < 0 || static_cast<std::size_t>(meshIndex) >= meshes.size()) continue;
            const glm::mat4& world = worldMatrices[i];
            for (const GltfPrimitive& prim : meshes[static_cast<std::size_t>(meshIndex)].primitives) {
                for (const glm::vec3& pos : prim.mesh.positions) {
                    const glm::vec4 transformed = world * glm::vec4(pos, 1.0f);
                    box.expand(glm::vec3(transformed));
                }
            }
        }
        return box;
    }
}
