#pragma once

#include <mge/core/math/Geometry.h>
#include <mge/procgen/MeshData.h>

#include <glm/glm.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace mge {

    // Single world-space triangle stored in a static MeshCollider.
    struct CollisionTriangle {
        glm::vec3 a{0.0f};
        glm::vec3 b{0.0f};
        glm::vec3 c{0.0f};
        glm::vec3 normal{0.0f, 1.0f, 0.0f};
        Aabb bounds;
    };

    // Upright capsule aligned with +Y. `feet` is the bottom-most point of the lower hemisphere.
    struct Capsule {
        glm::vec3 feet{0.0f};
        float radius = 0.35f;
        float height = 1.8f;

        glm::vec3 bottomCenter() const { return feet + glm::vec3(0.0f, radius, 0.0f); }
        glm::vec3 topCenter() const {
            return feet + glm::vec3(0.0f, std::max(height - radius, radius), 0.0f);
        }

        Aabb bounds() const {
            const float totalHeight = std::max(height, 2.0f * radius);
            return Aabb(feet + glm::vec3(-radius, 0.0f, -radius),
                        feet + glm::vec3(radius, totalHeight, radius));
        }
    };

    // Result of a raycast query against a MeshCollider.
    struct MeshRaycastHit {
        float distance = 0.0f;
        glm::vec3 point{0.0f};
        glm::vec3 normal{0.0f, 1.0f, 0.0f}; // Oriented towards the ray origin
        std::uint32_t triangleIndex = 0;
    };

    // Result of a continuous sphere or capsule sweep against a MeshCollider.
    struct ShapeCastHit {
        bool hit = false;
        float fraction = 1.0f;                  // [0, 1] along delta where contact occurs
        glm::vec3 normal{0.0f, 1.0f, 0.0f};     // Contact normal pointing towards the shape
        glm::vec3 faceNormal{0.0f, 1.0f, 0.0f}; // Triangle geometric normal oriented with `normal`
        glm::vec3 point{0.0f};                  // Closest point on the hit triangle
        std::uint32_t triangleIndex = 0;
    };

    // Single contact reported by an overlap query between a shape and a MeshCollider.
    struct ShapeContact {
        bool hit = false;
        glm::vec3 normal{0.0f, 1.0f, 0.0f};     // Points from triangle towards shape
        glm::vec3 faceNormal{0.0f, 1.0f, 0.0f}; // Triangle geometric normal oriented with `normal`
        glm::vec3 point{0.0f};
        float penetration = 0.0f;
        std::uint32_t triangleIndex = 0;
    };

    // Static triangle mesh collider with a flat BVH for fast AABB, raycast, shape-cast, and overlap queries.
    // Call build() after adding triangles/meshes (fromMesh() builds automatically).
    class MeshCollider {
    public:
        void clear();
        void addTriangle(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c);
        void addMesh(const MeshData& mesh, const glm::mat4& transform = glm::mat4(1.0f));
        void build();

        static MeshCollider fromMesh(const MeshData& mesh, const glm::mat4& transform = glm::mat4(1.0f));

        bool empty() const { return m_triangles.empty(); }
        std::size_t triangleCount() const { return m_triangles.size(); }
        const CollisionTriangle& triangle(std::size_t index) const { return m_triangles[index]; }
        Aabb bounds() const { return m_bounds; }

        // Appends indices of triangles whose AABB intersects queryBox (clears outIndices first).
        void queryAabb(const Aabb& queryBox, std::vector<std::uint32_t>& outIndices) const;

        // BVH-accelerated closest ray intersection within [0, maxDistance].
        std::optional<MeshRaycastHit> raycast(const Ray& ray, float maxDistance) const;

        // Continuous swept sphere / capsule cast along `delta` with optional scratch candidate buffer.
        ShapeCastHit sphereCast(const Sphere& sphere, const glm::vec3& delta, float skin = 1e-3f) const;
        ShapeCastHit sphereCast(const Sphere& sphere, const glm::vec3& delta, float skin,
                                std::vector<std::uint32_t>& candidates) const;

        ShapeCastHit capsuleCast(const Capsule& capsule, const glm::vec3& delta, float skin = 1e-3f) const;
        ShapeCastHit capsuleCast(const Capsule& capsule, const glm::vec3& delta, float skin,
                                 std::vector<std::uint32_t>& candidates) const;

        // Overlap queries: populate `outContacts` (cleared first) with all intersecting triangles.
        bool overlapSphere(const Sphere& sphere, std::vector<ShapeContact>& outContacts,
                           float extraRadius = 0.0f) const;
        bool overlapSphere(const Sphere& sphere, std::vector<ShapeContact>& outContacts, float extraRadius,
                           std::vector<std::uint32_t>& candidates) const;

        bool overlapCapsule(const Capsule& capsule, std::vector<ShapeContact>& outContacts,
                            float extraRadius = 0.0f) const;
        bool overlapCapsule(const Capsule& capsule, std::vector<ShapeContact>& outContacts, float extraRadius,
                            std::vector<std::uint32_t>& candidates) const;

    private:
        struct BvhNode {
            Aabb bounds;
            std::uint32_t leftFirst = 0;
            std::uint32_t count = 0; // 0 = internal node (leftFirst = left child, leftFirst + 1 = right)
        };

        void buildNode(std::uint32_t nodeIndex);

        std::vector<CollisionTriangle> m_triangles;
        std::vector<std::uint32_t> m_triIndices;
        std::vector<BvhNode> m_nodes;
        Aabb m_bounds;
    };

} // namespace mge
