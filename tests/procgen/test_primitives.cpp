#include <doctest/doctest.h>
#include <mge/procgen/Primitives.h>

#include "MeshChecks.h"

using namespace mge;
using namespace mge_test;

namespace {
    // Lo que debe cumplir cualquier primitiva
    void checkCommon(const MeshData& mesh, std::size_t vertices, std::size_t triangles) {
        CHECK(mesh.validate().empty());
        CHECK(mesh.vertexCount() == vertices);
        CHECK(mesh.triangleCount() == triangles);
        CHECK(normalsAreUnit(mesh));
        CHECK(windingMatchesNormals(mesh));
    }

    void checkBounds(const MeshData& mesh, const glm::vec3& min, const glm::vec3& max) {
        Aabb box = mesh.bounds();
        for (int axis = 0; axis < 3; axis++) {
            CHECK(box.min[axis] == doctest::Approx(min[axis]).epsilon(1e-4));
            CHECK(box.max[axis] == doctest::Approx(max[axis]).epsilon(1e-4));
        }
    }
}

TEST_CASE("plane: grilla, normal +Y, abierto") {
    MeshData mesh = primitives::plane(4.0f, 2.0f, 4, 2);
    checkCommon(mesh, 15, 16);
    checkBounds(mesh, {-2, 0, -1}, {2, 0, 1});
    for (const glm::vec3& n : mesh.normals) CHECK(n == glm::vec3(0, 1, 0));
    CHECK_FALSE(isWatertight(mesh)); // Superficie abierta
}

TEST_CASE("box: 24 vértices, 12 triángulos, hermética y normales hacia afuera") {
    MeshData mesh = primitives::box({2, 4, 6}, {1, 0, 0, 1});
    checkCommon(mesh, 24, 12);
    checkBounds(mesh, {-1, -2, -3}, {1, 2, 3});
    CHECK(isWatertight(mesh));
    CHECK(normalsPointOutward(mesh));
    CHECK(mesh.colors[5] == glm::vec4(1, 0, 0, 1));
}

TEST_CASE("uvSphere: conteos, hermética, radio y normales") {
    const int segments = 16, rings = 8;
    MeshData mesh = primitives::uvSphere(2.0f, segments, rings);
    checkCommon(mesh, (rings + 1) * (segments + 1), static_cast<std::size_t>(2 * segments * (rings - 1)));
    checkBounds(mesh, {-2, -2, -2}, {2, 2, 2});
    CHECK(isWatertight(mesh));
    CHECK(normalsPointOutward(mesh));
    for (const glm::vec3& p : mesh.positions) CHECK(glm::length(p) == doctest::Approx(2.0f).epsilon(1e-4));
}

TEST_CASE("icosphere: 10·4^n + 2 vértices, 20·4^n triángulos, hermética") {
    for (int n = 0; n <= 3; n++) {
        CAPTURE(n);
        const std::size_t power = static_cast<std::size_t>(1) << (2 * n);
        MeshData mesh = primitives::icosphere(1.5f, n);
        checkCommon(mesh, 10 * power + 2, 20 * power);
        CHECK(isWatertight(mesh));
        CHECK(normalsPointOutward(mesh));
        for (const glm::vec3& p : mesh.positions)
            CHECK(glm::length(p) == doctest::Approx(1.5f).epsilon(1e-4));
    }
}

TEST_CASE("cylinder: con tapas hermético, sin tapas abierto") {
    const int segments = 12;
    MeshData capped = primitives::cylinder(1.0f, 3.0f, segments, true);
    checkCommon(capped, 4 * segments + 6, 4 * segments);
    checkBounds(capped, {-1, -1.5f, -1}, {1, 1.5f, 1});
    CHECK(isWatertight(capped));
    CHECK(normalsPointOutward(capped));

    MeshData open = primitives::cylinder(1.0f, 3.0f, segments, false);
    checkCommon(open, 2 * (segments + 1), 2 * segments);
    CHECK_FALSE(isWatertight(open));
}

TEST_CASE("cone: hermético, normales laterales inclinadas hacia afuera") {
    const int segments = 12;
    MeshData mesh = primitives::cone(1.0f, 2.0f, segments);
    checkCommon(mesh, 3 * segments + 4, 2 * segments);
    checkBounds(mesh, {-1, -1, -1}, {1, 1, 1});
    CHECK(isWatertight(mesh));
    // Normal lateral de un cono de radio 1 y altura 2: (2, 1)/√5 en (radial, y)
    CHECK(mesh.normals[0].y == doctest::Approx(1.0f / std::sqrt(5.0f)));
}

TEST_CASE("torus: conteos, hermético, normales hacia afuera del tubo") {
    const int segments = 24, sides = 12;
    const float major = 3.0f, minor = 1.0f;
    MeshData mesh = primitives::torus(major, minor, segments, sides);
    checkCommon(mesh, (sides + 1) * (segments + 1), static_cast<std::size_t>(2 * segments * sides));
    checkBounds(mesh, {-4, -1, -4}, {4, 1, 4});
    CHECK(isWatertight(mesh));
    for (std::size_t i = 0; i < mesh.vertexCount(); i++) {
        const glm::vec3& p = mesh.positions[i];
        // Punto más cercano del círculo central del tubo
        glm::vec3 ring = glm::normalize(glm::vec3(p.x, 0.0f, p.z)) * major;
        CHECK(glm::dot(mesh.normals[i], p - ring) > 0.0f);
        CHECK(glm::length(p - ring) == doctest::Approx(minor).epsilon(1e-4));
    }
}

TEST_CASE("capsule: conteos, hermética, altura total = h + 2r") {
    const int segments = 16, rings = 4;
    MeshData mesh = primitives::capsule(0.5f, 2.0f, segments, rings);
    checkCommon(mesh, static_cast<std::size_t>(2 * (rings + 1) * (segments + 1)),
                static_cast<std::size_t>(4 * segments * rings));
    checkBounds(mesh, {-0.5f, -1.5f, -0.5f}, {0.5f, 1.5f, 0.5f});
    CHECK(isWatertight(mesh));
    CHECK(normalsPointOutward(mesh));
}

TEST_CASE("Parámetros inválidos se corrigen a valores mínimos seguros") {
    CHECK(primitives::uvSphere(1.0f, 1, 1).validate().empty());
    CHECK(primitives::plane(1.0f, 1.0f, 0, -3).triangleCount() == 2);
    CHECK(primitives::cylinder(1.0f, 1.0f, 2).validate().empty());
}
