#include <doctest/doctest.h>
#include <mge/core/math/Geometry.h>

#include <glm/gtc/matrix_transform.hpp>

using namespace mge;

namespace {
    // Cámara en el origen mirando a -Z, FOV 90°, aspecto 1, near 0.1, far 100
    Frustum testFrustum() {
        glm::mat4 projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 100.0f);
        glm::mat4 view =
            glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        return Frustum::fromViewProjection(projection * view);
    }
}

TEST_CASE("Aabb: vacía por defecto, expand/merge, contains con bordes, intersects") {
    Aabb empty;
    CHECK(empty.isEmpty());
    CHECK_FALSE(empty.contains(glm::vec3(0.0f)));

    Aabb box = Aabb::fromPoints({{1, 2, 3}, {-1, 0, 5}});
    CHECK(box.min == glm::vec3(-1, 0, 3));
    CHECK(box.max == glm::vec3(1, 2, 5));
    CHECK(box.center() == glm::vec3(0, 1, 4));
    CHECK(box.extents() == glm::vec3(1, 1, 1));
    CHECK(box.contains({1, 2, 5})); // Borde incluido
    CHECK_FALSE(box.contains({1.01f, 2, 5}));

    Aabb touching(glm::vec3(1, 0, 3), glm::vec3(2, 1, 4));
    CHECK(box.intersects(touching)); // Compartir una cara cuenta
    Aabb apart(glm::vec3(5, 5, 5), glm::vec3(6, 6, 6));
    CHECK_FALSE(box.intersects(apart));
    CHECK_FALSE(box.intersects(empty));

    Aabb merged = box;
    merged.merge(apart);
    CHECK(merged.max == glm::vec3(6, 6, 6));
    merged.merge(empty); // Unir con una caja vacía no cambia nada
    CHECK(merged.min == glm::vec3(-1, 0, 3));
    CHECK(Aabb::fromCenterExtents({0, 0, 0}, {1, 2, 3}).size() == glm::vec3(2, 4, 6));
}

TEST_CASE("Sphere: contains, esfera-esfera y esfera-caja") {
    Sphere s{{0, 0, 0}, 2.0f};
    CHECK(s.contains({0, 2, 0}));
    CHECK_FALSE(s.contains({0, 2.01f, 0}));
    CHECK(s.intersects(Sphere{{3.9f, 0, 0}, 2.0f}));
    CHECK_FALSE(s.intersects(Sphere{{4.1f, 0, 0}, 2.0f}));
    CHECK(s.intersects(Aabb({1.5f, -1, -1}, {3, 1, 1})));
    CHECK_FALSE(s.intersects(Aabb({1.5f, 1.5f, 1.5f}, {3, 3, 3}))); // La esquina más cercana está a 2.6
}

TEST_CASE("Plane: distancia con signo") {
    Plane plane = Plane::fromPointNormal({0, 5, 0}, {0, 2, 0}); // La normal se normaliza
    CHECK(plane.distance({0, 7, 0}) == doctest::Approx(2.0f));
    CHECK(plane.distance({3, 4, -1}) == doctest::Approx(-1.0f));
}

TEST_CASE("Frustum: puntos adentro, detrás, más allá del far y fuera del FOV") {
    Frustum frustum = testFrustum();
    CHECK(frustum.contains({0, 0, -10}));
    CHECK_FALSE(frustum.contains({0, 0, 10}));     // Detrás de la cámara
    CHECK_FALSE(frustum.contains({0, 0, -200}));   // Más allá del far
    CHECK_FALSE(frustum.contains({0, 0, -0.05f})); // Antes del near
    CHECK(frustum.contains({9.9f, 0, -10}));       // FOV 90°: a z=-10 el borde está en x=10
    CHECK_FALSE(frustum.contains({10.5f, 0, -10}));
}

TEST_CASE("Frustum: esferas y cajas, incluidas las que cruzan un plano") {
    Frustum frustum = testFrustum();
    CHECK(frustum.intersects(Sphere{{0, 0, -50}, 1.0f}));
    CHECK_FALSE(frustum.intersects(Sphere{{0, 0, 50}, 1.0f}));
    CHECK(frustum.intersects(Sphere{{11, 0, -10}, 2.0f})); // Asoma por el borde derecho
    CHECK_FALSE(frustum.intersects(Sphere{{20, 0, -10}, 2.0f}));

    CHECK(frustum.intersects(Aabb({-1, -1, -11}, {1, 1, -9})));
    CHECK(frustum.intersects(Aabb({9, -1, -11}, {12, 1, -9})));    // Parcialmente adentro
    CHECK_FALSE(frustum.intersects(Aabb({-1, -1, 5}, {1, 1, 9}))); // Detrás
    CHECK_FALSE(frustum.intersects(Aabb()));                       // Vacía
}

TEST_CASE("Rayo-caja: impacto, fallo, rayo paralelo y origen adentro") {
    Aabb box({-1, -1, -1}, {1, 1, 1});
    auto hit = intersectRayAabb(Ray({-5, 0, 0}, {1, 0, 0}), box);
    REQUIRE(hit);
    CHECK(*hit == doctest::Approx(4.0f));
    CHECK_FALSE(intersectRayAabb(Ray({-5, 3, 0}, {1, 0, 0}), box));
    CHECK_FALSE(intersectRayAabb(Ray({-5, 0, 0}, {-1, 0, 0}), box)); // Apunta al otro lado
    CHECK(intersectRayAabb(Ray({-5, 0.5f, 0}, {1, 0, 0}), box));     // Paralelo a Y/Z, dentro de su rango
    CHECK(*intersectRayAabb(Ray({0, 0, 0}, {0, 1, 0}), box) == doctest::Approx(0.0f)); // Desde adentro
}

TEST_CASE("Rayo-esfera y rayo-plano") {
    Sphere sphere{{0, 0, -10}, 2.0f};
    auto hit = intersectRaySphere(Ray({0, 0, 0}, {0, 0, -1}), sphere);
    REQUIRE(hit);
    CHECK(*hit == doctest::Approx(8.0f));
    CHECK_FALSE(intersectRaySphere(Ray({0, 3, 0}, {0, 0, -1}), sphere));
    CHECK_FALSE(intersectRaySphere(Ray({0, 0, 0}, {0, 0, 1}), sphere)); // Se aleja
    CHECK(*intersectRaySphere(Ray({0, 0, -10}, {1, 0, 0}), sphere) == doctest::Approx(0.0f));

    Plane ground = Plane::fromPointNormal({0, 0, 0}, {0, 1, 0});
    auto t = intersectRayPlane(Ray({0, 10, 0}, {0, -1, 0}), ground);
    REQUIRE(t);
    CHECK(*t == doctest::Approx(10.0f));
    CHECK_FALSE(intersectRayPlane(Ray({0, 10, 0}, {1, 0, 0}), ground)); // Paralelo
    CHECK_FALSE(intersectRayPlane(Ray({0, 10, 0}, {0, 1, 0}), ground)); // Se aleja
}

TEST_CASE("Rayo-triángulo: interior, bordes, afuera, paralelo, doble cara") {
    glm::vec3 a(0, 0, 0), b(1, 0, 0), c(0, 1, 0);
    auto hit = intersectRayTriangle(Ray({0.25f, 0.25f, 5}, {0, 0, -1}), a, b, c);
    REQUIRE(hit);
    CHECK(*hit == doctest::Approx(5.0f));
    CHECK(intersectRayTriangle(Ray({0.5f, 0.5f, 5}, {0, 0, -1}), a, b, c)); // Sobre la hipotenusa
    CHECK(intersectRayTriangle(Ray({0, 0, 5}, {0, 0, -1}), a, b, c));       // Vértice
    CHECK_FALSE(intersectRayTriangle(Ray({0.6f, 0.6f, 5}, {0, 0, -1}), a, b, c));
    CHECK_FALSE(intersectRayTriangle(Ray({0.2f, 0.2f, 5}, {1, 0, 0}), a, b, c));   // Paralelo
    CHECK(intersectRayTriangle(Ray({0.25f, 0.25f, -5}, {0, 0, 1}), a, b, c));      // Por detrás
    CHECK_FALSE(intersectRayTriangle(Ray({0.25f, 0.25f, 5}, {0, 0, 1}), a, b, c)); // Detrás del origen
}
