#include <doctest/doctest.h>
#include <mge/procgen/MeshOps.h>
#include <mge/procgen/Primitives.h>

#include "MeshChecks.h"

#include <glm/gtc/matrix_transform.hpp>

using namespace mge;
using namespace mge_test;

namespace {
    // 6-vertex L-shape (concave), counter-clockwise, area 3
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

TEST_CASE("transformMesh: non-uniform scale keeps normals unit-length and perpendicular to the surface") {
    MeshData mesh = primitives::uvSphere(1.0f, 24, 12);
    transformMesh(mesh, glm::scale(glm::translate(glm::mat4(1.0f), {5, 0, 0}), {3.0f, 1.0f, 0.5f}));
    CHECK(normalsAreUnit(mesh));
    CHECK(windingMatchesNormals(mesh));
    // On an ellipsoid (x/a)²+(y/b)²+(z/c)² = 1 the normal is proportional to (x/a², y/b², z/c²)
    for (std::size_t i = 0; i < mesh.vertexCount(); i++) {
        glm::vec3 p = mesh.positions[i] - glm::vec3(5, 0, 0);
        glm::vec3 expected = glm::normalize(glm::vec3(p.x / 9.0f, p.y / 1.0f, p.z / 0.25f));
        CHECK(glm::dot(mesh.normals[i], expected) == doctest::Approx(1.0f).epsilon(1e-3));
    }
    CHECK(mesh.bounds().center().x == doctest::Approx(5.0f).epsilon(1e-4));
}

TEST_CASE("transformMesh: a mirror (negative determinant) preserves counter-clockwise winding") {
    MeshData mesh = primitives::box({1, 1, 1});
    transformMesh(mesh, glm::scale(glm::mat4(1.0f), {-1.0f, 1.0f, 1.0f}));
    CHECK(windingMatchesNormals(mesh));
    CHECK(isWatertight(mesh));
    CHECK(normalsPointOutward(mesh));
}

TEST_CASE("appendMesh/mergeMeshes offset indices") {
    MeshData a = primitives::box({1, 1, 1});
    MeshData b = primitives::icosphere(1.0f, 0);
    MeshData merged = mergeMeshes(a, b);
    CHECK(merged.vertexCount() == a.vertexCount() + b.vertexCount());
    CHECK(merged.triangleCount() == a.triangleCount() + b.triangleCount());
    CHECK(merged.validate().empty());
    CHECK(merged.indices[a.indices.size()] == b.indices[0] + a.vertexCount());
}

TEST_CASE("weldVertices merges exact duplicates while preserving hard edges") {
    MeshData box = primitives::box({1, 1, 1});
    MeshData doubled = mergeMeshes(box, box);
    weldVertices(doubled);
    CHECK(doubled.vertexCount() == 24); // Corners have 3 distinct normals: they are not merged
    CHECK(doubled.triangleCount() == 24);
    CHECK(doubled.validate().empty());

    // Seams: UV sphere duplicates have different UVs, so they are preserved
    MeshData sphere = primitives::uvSphere(1.0f, 8, 4);
    const std::size_t before = sphere.vertexCount();
    weldVertices(sphere);
    CHECK(sphere.vertexCount() == before);

    // Exact duplicates (same attributes) are merged
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

TEST_CASE("flipWinding flips normals and triangles consistently") {
    MeshData mesh = primitives::icosphere(1.0f, 1);
    const glm::vec3 firstNormal = mesh.normals[0];
    flipWinding(mesh);
    CHECK(mesh.normals[0] == -firstNormal);
    CHECK(windingMatchesNormals(mesh));
    CHECK(isWatertight(mesh));
}

TEST_CASE("recomputeNormals: Smooth recovers sphere normals, Flat produces faceted normals") {
    MeshData mesh = primitives::uvSphere(2.0f, 32, 16);
    for (glm::vec3& n : mesh.normals) n = glm::vec3(0, 1, 0); // Corrupt them
    recomputeNormals(mesh, NormalMode::Smooth);
    for (std::size_t i = 0; i < mesh.vertexCount(); i++) {
        CHECK(glm::dot(mesh.normals[i], glm::normalize(mesh.positions[i])) > 0.99f);
    }

    MeshData flat = primitives::icosphere(1.0f, 1);
    recomputeNormals(flat, NormalMode::Flat);
    CHECK(flat.vertexCount() == flat.triangleCount() * 3);
    CHECK(windingMatchesNormals(flat));
    CHECK(isWatertight(flat));
    CHECK(flat.normals[0] == flat.normals[1]); // Same triangle, same normal
}

TEST_CASE("triangulatePolygon: convex, concave, clockwise, and degenerate cases") {
    std::vector<glm::vec2> square = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    CHECK(triangulatePolygon(square).size() == 6);
    CHECK(trianglesArea(square, triangulatePolygon(square)) == doctest::Approx(1.0f));

    auto lTriangles = triangulatePolygon(kLShape);
    CHECK(lTriangles.size() == 12);
    CHECK(trianglesArea(kLShape, lTriangles) == doctest::Approx(3.0f));

    std::vector<glm::vec2> clockwise(kLShape.rbegin(), kLShape.rend());
    auto cwTriangles = triangulatePolygon(clockwise);
    CHECK(cwTriangles.size() == 12);
    CHECK(trianglesArea(clockwise, cwTriangles) == doctest::Approx(3.0f)); // Counter-clockwise triangles

    CHECK(triangulatePolygon({{0, 0}, {1, 0}}).empty());
}

TEST_CASE("extrude of a concave L-shape: watertight, counts, and bounds") {
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

TEST_CASE("lathe: straight profile yields exact-radius cylinder; with poles it is closed") {
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

TEST_CASE("sweep of a square along a straight path equals a prism") {
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

TEST_CASE("sweep along a curve and along a closed path (ring)") {
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
    CHECK(isWatertight(donut)); // Closed path: no caps and still watertight
}
