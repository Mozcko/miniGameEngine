#pragma once

#include <glm/glm.hpp>

#include <array>
#include <initializer_list>
#include <optional>

namespace mge {

    // Axis-aligned bounding box. Empty by default (min > max): merge/expand populate it.
    struct Aabb {
        glm::vec3 min{1e30f};
        glm::vec3 max{-1e30f};

        Aabb() = default;
        Aabb(const glm::vec3& minCorner, const glm::vec3& maxCorner) : min(minCorner), max(maxCorner) {}

        static Aabb fromPoints(std::initializer_list<glm::vec3> points);
        static Aabb fromCenterExtents(const glm::vec3& center, const glm::vec3& halfExtents);

        bool isEmpty() const { return min.x > max.x || min.y > max.y || min.z > max.z; }
        glm::vec3 center() const { return (min + max) * 0.5f; }
        glm::vec3 extents() const { return (max - min) * 0.5f; } // Half size
        glm::vec3 size() const { return max - min; }

        void expand(const glm::vec3& point);
        void merge(const Aabb& other);
        bool contains(const glm::vec3& point) const; // Boundary included
        bool intersects(const Aabb& other) const;    // Touching counts as intersection
    };

    struct Sphere {
        glm::vec3 center{0.0f};
        float radius = 0.0f;

        bool contains(const glm::vec3& point) const;
        bool intersects(const Sphere& other) const;
        bool intersects(const Aabb& box) const;
    };

    // Plane: dot(normal, p) + d = 0. Unit normal; signed distance is positive on the normal side.
    struct Plane {
        glm::vec3 normal{0.0f, 1.0f, 0.0f};
        float d = 0.0f;

        static Plane fromPointNormal(const glm::vec3& point, const glm::vec3& normal);
        float distance(const glm::vec3& point) const { return glm::dot(normal, point) + d; }
    };

    // Ray: origin + t * direction (direction is normalized in the constructor)
    struct Ray {
        glm::vec3 origin{0.0f};
        glm::vec3 direction{0.0f, 0.0f, -1.0f};

        Ray() = default;
        Ray(const glm::vec3& rayOrigin, const glm::vec3& rayDirection);
        glm::vec3 at(float t) const { return origin + direction * t; }
    };

    // Projection Z range: [0, 1] (SDL GPU, Vulkan, D3D, Metal) or [-1, 1] (OpenGL, glm::perspective)
    enum class DepthRange {
        ZeroToOne,
        MinusOneToOne
    };

    // Camera frustum: 6 planes with inward-pointing normals (left, right, bottom, top, near, far)
    class Frustum {
    public:
        // Extracts planes from projection * view (Gribb–Hartmann). Only the near plane differs:
        // z >= 0 with ZeroToOne (default, used by the engine) or z >= -w with MinusOneToOne.
        static Frustum fromViewProjection(const glm::mat4& viewProjection,
                                          DepthRange depthRange = DepthRange::ZeroToOne);

        bool contains(const glm::vec3& point) const;
        // Conservative: may return true for boxes/spheres slightly outside near corners
        bool intersects(const Sphere& sphere) const;
        bool intersects(const Aabb& box) const;

        const std::array<Plane, 6>& planes() const { return m_planes; }

    private:
        std::array<Plane, 6> m_planes{};
    };

    // --- Ray intersections: return distance t >= 0 to the first hit ---
    // If the origin is inside the box/sphere, t = 0.
    std::optional<float> intersectRayAabb(const Ray& ray, const Aabb& box);
    std::optional<float> intersectRaySphere(const Ray& ray, const Sphere& sphere);
    std::optional<float> intersectRayPlane(const Ray& ray, const Plane& plane); // nullopt if parallel
    // Möller–Trumbore, double-sided. nullopt if the ray is parallel or misses the triangle.
    std::optional<float> intersectRayTriangle(const Ray& ray, const glm::vec3& a, const glm::vec3& b,
                                              const glm::vec3& c);
}
