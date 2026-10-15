#pragma once

#include <glm/glm.hpp>

#include <array>
#include <initializer_list>
#include <optional>

namespace mge {

    // Caja alineada a los ejes. Vacía por defecto (min > max): merge/expand la llenan.
    struct Aabb {
        glm::vec3 min{1e30f};
        glm::vec3 max{-1e30f};

        Aabb() = default;
        Aabb(const glm::vec3& minCorner, const glm::vec3& maxCorner) : min(minCorner), max(maxCorner) {}

        static Aabb fromPoints(std::initializer_list<glm::vec3> points);
        static Aabb fromCenterExtents(const glm::vec3& center, const glm::vec3& halfExtents);

        bool isEmpty() const { return min.x > max.x || min.y > max.y || min.z > max.z; }
        glm::vec3 center() const { return (min + max) * 0.5f; }
        glm::vec3 extents() const { return (max - min) * 0.5f; } // Mitad del tamaño
        glm::vec3 size() const { return max - min; }

        void expand(const glm::vec3& point);
        void merge(const Aabb& other);
        bool contains(const glm::vec3& point) const; // Bordes incluidos
        bool intersects(const Aabb& other) const;    // Tocarse cuenta como intersección
    };

    struct Sphere {
        glm::vec3 center{0.0f};
        float radius = 0.0f;

        bool contains(const glm::vec3& point) const;
        bool intersects(const Sphere& other) const;
        bool intersects(const Aabb& box) const;
    };

    // Plano: dot(normal, p) + d = 0. normal unitaria; distancia con signo positiva del lado de la normal.
    struct Plane {
        glm::vec3 normal{0.0f, 1.0f, 0.0f};
        float d = 0.0f;

        static Plane fromPointNormal(const glm::vec3& point, const glm::vec3& normal);
        float distance(const glm::vec3& point) const { return glm::dot(normal, point) + d; }
    };

    // Rayo: origin + t * direction (direction se normaliza en el constructor)
    struct Ray {
        glm::vec3 origin{0.0f};
        glm::vec3 direction{0.0f, 0.0f, -1.0f};

        Ray() = default;
        Ray(const glm::vec3& rayOrigin, const glm::vec3& rayDirection);
        glm::vec3 at(float t) const { return origin + direction * t; }
    };

    // Frustum de cámara: 6 planos con la normal hacia adentro (left, right, bottom, top, near, far)
    class Frustum {
    public:
        // Extrae los planos de projection * view (Gribb–Hartmann). Convención OpenGL (z en [-w, w]).
        static Frustum fromViewProjection(const glm::mat4& viewProjection);

        bool contains(const glm::vec3& point) const;
        // Conservador: puede devolver true para cajas/esferas apenas afuera cerca de las esquinas
        bool intersects(const Sphere& sphere) const;
        bool intersects(const Aabb& box) const;

        const std::array<Plane, 6>& planes() const { return m_planes; }

    private:
        std::array<Plane, 6> m_planes{};
    };

    // --- Intersecciones de rayos: devuelven la distancia t >= 0 al primer impacto ---
    // Si el origen está dentro de la caja/esfera, t = 0.
    std::optional<float> intersectRayAabb(const Ray& ray, const Aabb& box);
    std::optional<float> intersectRaySphere(const Ray& ray, const Sphere& sphere);
    std::optional<float> intersectRayPlane(const Ray& ray, const Plane& plane); // nullopt si es paralelo
    // Möller–Trumbore, doble cara. nullopt si el rayo es paralelo o no toca el triángulo.
    std::optional<float> intersectRayTriangle(const Ray& ray, const glm::vec3& a, const glm::vec3& b,
                                              const glm::vec3& c);
}
