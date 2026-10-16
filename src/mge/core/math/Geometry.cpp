#include <mge/core/math/Geometry.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace mge {

    // --- Aabb ---

    Aabb Aabb::fromPoints(std::initializer_list<glm::vec3> points) {
        Aabb box;
        for (const glm::vec3& p : points) box.expand(p);
        return box;
    }

    Aabb Aabb::fromCenterExtents(const glm::vec3& center, const glm::vec3& halfExtents) {
        return Aabb(center - halfExtents, center + halfExtents);
    }

    void Aabb::expand(const glm::vec3& point) {
        min = glm::min(min, point);
        max = glm::max(max, point);
    }

    void Aabb::merge(const Aabb& other) {
        if (other.isEmpty()) return;
        min = glm::min(min, other.min);
        max = glm::max(max, other.max);
    }

    bool Aabb::contains(const glm::vec3& p) const {
        return p.x >= min.x && p.x <= max.x && p.y >= min.y && p.y <= max.y && p.z >= min.z && p.z <= max.z;
    }

    bool Aabb::intersects(const Aabb& o) const {
        if (isEmpty() || o.isEmpty()) return false;
        return min.x <= o.max.x && max.x >= o.min.x && min.y <= o.max.y && max.y >= o.min.y &&
               min.z <= o.max.z && max.z >= o.min.z;
    }

    // --- Sphere ---

    bool Sphere::contains(const glm::vec3& point) const {
        glm::vec3 delta = point - center;
        return glm::dot(delta, delta) <= radius * radius;
    }

    bool Sphere::intersects(const Sphere& other) const {
        glm::vec3 delta = other.center - center;
        float sum = radius + other.radius;
        return glm::dot(delta, delta) <= sum * sum;
    }

    bool Sphere::intersects(const Aabb& box) const {
        if (box.isEmpty()) return false;
        glm::vec3 closest = glm::clamp(center, box.min, box.max);
        return contains(closest);
    }

    // --- Plane / Ray ---

    Plane Plane::fromPointNormal(const glm::vec3& point, const glm::vec3& normal) {
        glm::vec3 n = glm::normalize(normal);
        return Plane{n, -glm::dot(n, point)};
    }

    Ray::Ray(const glm::vec3& rayOrigin, const glm::vec3& rayDirection)
        : origin(rayOrigin), direction(glm::normalize(rayDirection)) {}

    // --- Frustum ---

    Frustum Frustum::fromViewProjection(const glm::mat4& m, DepthRange depthRange) {
        // Filas de la matriz (glm es column-major: m[columna][fila])
        auto row = [&m](int r) { return glm::vec4(m[0][r], m[1][r], m[2][r], m[3][r]); };
        const glm::vec4 r0 = row(0), r1 = row(1), r2 = row(2), r3 = row(3);
        const glm::vec4 nearPlane = depthRange == DepthRange::ZeroToOne ? r2 : r3 + r2;
        const glm::vec4 raw[6] = {r3 + r0, r3 - r0, r3 + r1, r3 - r1, nearPlane, r3 - r2};

        Frustum frustum;
        for (int i = 0; i < 6; i++) {
            glm::vec3 normal(raw[i]);
            float length = glm::length(normal);
            frustum.m_planes[static_cast<std::size_t>(i)] = Plane{normal / length, raw[i].w / length};
        }
        return frustum;
    }

    bool Frustum::contains(const glm::vec3& point) const {
        return std::all_of(m_planes.begin(), m_planes.end(),
                           [&](const Plane& plane) { return plane.distance(point) >= 0.0f; });
    }

    bool Frustum::intersects(const Sphere& sphere) const {
        return std::all_of(m_planes.begin(), m_planes.end(), [&](const Plane& plane) {
            return plane.distance(sphere.center) >= -sphere.radius;
        });
    }

    bool Frustum::intersects(const Aabb& box) const {
        if (box.isEmpty()) return false;
        // Para cada plano, el vértice de la caja más "adentro" (vértice positivo) debe estar adentro
        for (const Plane& plane : m_planes) {
            glm::vec3 positive(plane.normal.x >= 0.0f ? box.max.x : box.min.x,
                               plane.normal.y >= 0.0f ? box.max.y : box.min.y,
                               plane.normal.z >= 0.0f ? box.max.z : box.min.z);
            if (plane.distance(positive) < 0.0f) return false;
        }
        return true;
    }

    // --- Rayos ---

    std::optional<float> intersectRayAabb(const Ray& ray, const Aabb& box) {
        if (box.isEmpty()) return std::nullopt;
        float tMin = 0.0f;
        float tMax = std::numeric_limits<float>::max();
        for (int axis = 0; axis < 3; axis++) {
            const float origin = ray.origin[axis];
            const float dir = ray.direction[axis];
            if (std::abs(dir) < 1e-12f) {
                // Paralelo a las caras de este eje: tiene que estar entre ellas
                if (origin < box.min[axis] || origin > box.max[axis]) return std::nullopt;
                continue;
            }
            float t1 = (box.min[axis] - origin) / dir;
            float t2 = (box.max[axis] - origin) / dir;
            if (t1 > t2) std::swap(t1, t2);
            tMin = std::max(tMin, t1);
            tMax = std::min(tMax, t2);
            if (tMin > tMax) return std::nullopt;
        }
        return tMin;
    }

    std::optional<float> intersectRaySphere(const Ray& ray, const Sphere& sphere) {
        glm::vec3 toOrigin = ray.origin - sphere.center;
        float b = glm::dot(toOrigin, ray.direction);
        float c = glm::dot(toOrigin, toOrigin) - sphere.radius * sphere.radius;
        if (c <= 0.0f) return 0.0f;        // El origen está dentro
        if (b > 0.0f) return std::nullopt; // Afuera y alejándose
        float discriminant = b * b - c;
        if (discriminant < 0.0f) return std::nullopt;
        return -b - std::sqrt(discriminant);
    }

    std::optional<float> intersectRayPlane(const Ray& ray, const Plane& plane) {
        float denominator = glm::dot(plane.normal, ray.direction);
        if (std::abs(denominator) < 1e-8f) return std::nullopt;
        float t = -plane.distance(ray.origin) / denominator;
        if (t < 0.0f) return std::nullopt;
        return t;
    }

    std::optional<float> intersectRayTriangle(const Ray& ray, const glm::vec3& a, const glm::vec3& b,
                                              const glm::vec3& c) {
        constexpr float kEpsilon = 1e-8f;
        glm::vec3 edge1 = b - a;
        glm::vec3 edge2 = c - a;
        glm::vec3 p = glm::cross(ray.direction, edge2);
        float det = glm::dot(edge1, p);
        if (std::abs(det) < kEpsilon) return std::nullopt;
        float inverseDet = 1.0f / det;
        glm::vec3 s = ray.origin - a;
        float u = glm::dot(s, p) * inverseDet;
        if (u < 0.0f || u > 1.0f) return std::nullopt;
        glm::vec3 q = glm::cross(s, edge1);
        float v = glm::dot(ray.direction, q) * inverseDet;
        if (v < 0.0f || u + v > 1.0f) return std::nullopt;
        float t = glm::dot(edge2, q) * inverseDet;
        if (t < 0.0f) return std::nullopt;
        return t;
    }
}
