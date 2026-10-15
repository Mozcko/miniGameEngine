#include <doctest/doctest.h>
#include <mge/procgen/MeshOps.h>
#include <mge/procgen/Primitives.h>

#include "MeshChecks.h"

#include <glm/gtc/matrix_transform.hpp>

using namespace mge;
using namespace mge_test;

namespace {
    // L de 6 vértices (cóncava), antihoraria, área 3
    const std::vector<glm::vec2> kLShape = {{0, 0}, {2, 0}, {2, 1}, {1, 1}, {1, 2}, {0, 2}};

    float trianglesArea(const std::vector<glm::vec2>& polygon, const std::vector<std::uint32_t>& triangles) {
        float area = 0.0f;
        for (std::size_t t = 0; t + 2 < triangles.size(); t += 3) {
            const glm::vec2 a = polygon[triangles[t]], b = polygon[triangles[t + 1]],
                            c = polygon[triangles[t + 2]];
            area += 0.5f * ((b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y));
        }
        return area;
    }
}

TEST_CASE("transformMesh: escala no uniforme mantiene normales unitarias y perpendiculares a la superficie") {
    MeshData mesh = primitives::uvSphere(1.0f, 24, 12);
    transformMesh(mesh, glm::scale(glm::translate(glm::mat4(1.0f), {5, 0, 0}), {3.0f, 1.0f, 0.5f}));
    CHECK(normalsAreUnit(mesh));
    CHECK(windingMatchesNormals(mesh));
    // En un elipsoide (x/a)²+(y/b)²+(z/c)² = 1 la normal es proporcional a (x/a², y/b², z/c²)
    for (std::size_t i = 0; i < mesh.vertexCount(); i++) {
        glm::vec3 p = mesh.positions[i] - glm::vec3(5, 0, 0);
        glm::vec3 expected = glm::normalize(glm::vec3(p.x / 9.0f, p.y / 1.0f, p.z / 0.25f));
        CHECK(glm::dot(mesh.normals[i], expected) == doctest::Approx(1.0f).epsilon(1e-3));
    }
    CHECK(mesh.bounds().center().x == doctest::Approx(5.0f).epsilon(1e-4));
}

TEST_CASE("transformMesh: un espejo (determinante negativo) conserva el sentido antihorario") {
    MeshData mesh = primitives::box({1, 1, 1});
    transformMesh(mesh, glm::scale(glm::mat4(1.0f), {-1.0f, 1.0f, 1.0f}));
    CHECK(windingMatchesNormals(mesh));
    CHECK(isWatertight(mesh));
    CHECK(normalsPointOutward(mesh));
}

TEST_CASE("appendMesh/mergeMeshes desplazan índices") {
    MeshData a = primitives::box({1, 1, 1});
    MeshData b = primitives::icosphere(1.0f, 0);
    MeshData merged = mergeMeshes(a, b);
    CHECK(merged.vertexCount() == a.vertexCount() + b.vertexCount());
    CHECK(merged.triangleCount() == a.triangleCount() + b.triangleCount());
    CHECK(merged.validate().empty());
    CHECK(merged.indices[a.indices.size()] == b.indices[0] + a.vertexCount());
}

TEST_CASE("weldVertices une duplicados exactos pero conserva aristas duras") {
    MeshData box = primitives::box({1, 1, 1});
    MeshData doubled = mergeMeshes(box, box);
    weldVertices(doubled);
    CHECK(doubled.vertexCount() == 24); // Las esquinas tienen 3 normales distintas: no se unen
    CHECK(doubled.triangleCount() == 24);
    CHECK(doubled.validate().empty());

    // Costuras: los duplicados de la esfera UV tienen UV distinto, así que se conservan
    MeshData sphere = primitives::uvSphere(1.0f, 8, 4);
    const std::size_t before = sphere.vertexCount();
    weldVertices(sphere);
    CHECK(sphere.vertexCount() == before);

    // Duplicados exactos (mismos atributos) sí se unen
    MeshData strip;
    auto a = strip.addVertex({0, 0, 0});
    auto b = strip.addVertex({1, 0, 0});
    auto c = strip.addVertex({1, 1, 0});
    auto c2 = strip.addVertex({1, 1, 0});
    auto d = strip.addVertex({0, 1, 0});
    auto a2 = strip.addVertex({0, 0, 0});
    strip.addTriangle(a, b, c);
    strip.addTriangle(a2, c2, d);
    weldVertices(strip);
    CHECK(strip.vertexCount() == 4);
    CHECK(strip.indices == std::vector<std::uint32_t>{0, 1, 2, 0, 2, 3});
}

TEST_CASE("flipWinding invierte normales y triángulos de forma coherente") {
    MeshData mesh = primitives::icosphere(1.0f, 1);
    const glm::vec3 firstNormal = mesh.normals[0];
    flipWinding(mesh);
    CHECK(mesh.normals[0] == -firstNormal);
    CHECK(windingMatchesNormals(mesh));
    CHECK(isWatertight(mesh));
}

TEST_CASE("recomputeNormals: Smooth recupera las normales de la esfera, Flat facetada") {
    MeshData mesh = primitives::uvSphere(2.0f, 32, 16);
    for (glm::vec3& n : mesh.normals) n = glm::vec3(0, 1, 0); // Romperlas
    recomputeNormals(mesh, NormalMode::Smooth);
    for (std::size_t i = 0; i < mesh.vertexCount(); i++) {
        CHECK(glm::dot(mesh.normals[i], glm::normalize(mesh.positions[i])) > 0.99f);
    }

    MeshData flat = primitives::icosphere(1.0f, 1);
    recomputeNormals(flat, NormalMode::Flat);
    CHECK(flat.vertexCount() == flat.triangleCount() * 3);
    CHECK(windingMatchesNormals(flat));
    CHECK(isWatertight(flat));
    CHECK(flat.normals[0] == flat.normals[1]); // Mismo triángulo, misma normal
}

TEST_CASE("triangulatePolygon: convexo, cóncavo, sentido horario y casos degenerados") {
    std::vector<glm::vec2> square = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    CHECK(triangulatePolygon(square).size() == 6);
    CHECK(trianglesArea(square, triangulatePolygon(square)) == doctest::Approx(1.0f));

    auto lTriangles = triangulatePolygon(kLShape);
    CHECK(lTriangles.size() == 12);
    CHECK(trianglesArea(kLShape, lTriangles) == doctest::Approx(3.0f));

    std::vector<glm::vec2> clockwise(kLShape.rbegin(), kLShape.rend());
    auto cwTriangles = triangulatePolygon(clockwise);
    CHECK(cwTriangles.size() == 12);
    CHECK(trianglesArea(clockwise, cwTriangles) == doctest::Approx(3.0f)); // Triángulos antihorarios

    CHECK(triangulatePolygon({{0, 0}, {1, 0}}).empty());
}

TEST_CASE("extrude de una L cóncava: hermética, conteos y caja") {
    MeshData mesh = extrude(kLShape, 1.0f);
    CHECK(mesh.validate().empty());
    CHECK(mesh.triangleCount() == 2 * (6 - 2) + 2 * 6);
    CHECK(mesh.vertexCount() == 2 * 6 + 4 * 6);
    CHECK(isWatertight(mesh));
    CHECK(windingMatchesNormals(mesh));
    Aabb box = mesh.bounds();
    CHECK(box.min == glm::vec3(0, 0, -0.5f));
    CHECK(box.max == glm::vec3(2, 2, 0.5f));
}

TEST_CASE("lathe: un perfil recto da un cilindro de radio exacto; con polos queda cerrado") {
    MeshData open = lathe({{1, -1}, {1, 1}}, 16);
    for (const glm::vec3& p : open.positions)
        CHECK(glm::length(glm::vec2(p.x, p.z)) == doctest::Approx(1.0f));
    CHECK_FALSE(isWatertight(open));

    MeshData closed = lathe({{0, -1}, {1, -1}, {1, 1}, {0, 1}}, 16);
    CHECK(closed.validate().empty());
    CHECK(isWatertight(closed));
    CHECK(windingMatchesNormals(closed));
    CHECK(normalsPointOutward(closed));
}

TEST_CASE("sweep de un cuadrado por un camino recto equivale a un prisma") {
    std::vector<glm::vec2> square = {{-0.5f, -0.5f}, {0.5f, -0.5f}, {0.5f, 0.5f}, {-0.5f, 0.5f}};
    MeshData mesh = sweep(square, {{0, 0, 0}, {0, 0, 1}, {0, 0, 3}});
    CHECK(mesh.validate().empty());
    CHECK(isWatertight(mesh));
    CHECK(windingMatchesNormals(mesh));
    Aabb box = mesh.bounds();
    CHECK(box.size().x == doctest::Approx(1.0f));
    CHECK(box.size().y == doctest::Approx(1.0f));
    CHECK(box.size().z == doctest::Approx(3.0f));
}

TEST_CASE("sweep por una curva y por un camino cerrado (anillo)") {
    std::vector<glm::vec2> circle;
    for (int i = 0; i < 8; i++) {
        float a = static_cast<float>(i) / 8.0f * 6.2831853f;
        circle.push_back({0.2f * std::cos(a), 0.2f * std::sin(a)});
    }
    std::vector<glm::vec3> helix, ring;
    for (int i = 0; i < 32; i++) {
        float t = static_cast<float>(i) / 32.0f * 6.2831853f;
        helix.push_back({std::cos(t), t * 0.3f, std::sin(t)});
        ring.push_back({2.0f * std::cos(t), 0.0f, 2.0f * std::sin(t)});
    }
    MeshData spring = sweep(circle, helix);
    CHECK(spring.validate().empty());
    CHECK(isWatertight(spring));
    CHECK(windingMatchesNormals(spring));

    MeshData donut = sweep(circle, ring, /*closedPath=*/true);
    CHECK(donut.validate().empty());
    CHECK(isWatertight(donut)); // Camino cerrado: sin tapas y aun así cerrado
}
