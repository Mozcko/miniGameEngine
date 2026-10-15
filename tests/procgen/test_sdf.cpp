#include <doctest/doctest.h>
#include <mge/procgen/Sdf.h>

#include "MeshChecks.h"

#include <glm/gtc/quaternion.hpp>

using namespace mge;
using namespace mge_test;

namespace {
    const Aabb kBounds({-2.0f, -2.0f, -2.0f}, {2.0f, 2.0f, 2.0f});
}

TEST_CASE("Distancias exactas de las primitivas") {
    CHECK(sdf::sphere(1.0f)({2, 0, 0}) == doctest::Approx(1.0f));
    CHECK(sdf::sphere(1.0f)({0, 0, 0}) == doctest::Approx(-1.0f));
    CHECK(sdf::box({1, 1, 1})({3, 0, 0}) == doctest::Approx(2.0f));
    CHECK(sdf::box({1, 1, 1})({2, 2, 1}) == doctest::Approx(std::sqrt(2.0f))); // Distancia a la arista
    CHECK(sdf::box({1, 2, 3})({0, 0, 0}) == doctest::Approx(-1.0f));
    CHECK(sdf::torus(2.0f, 0.5f)({2, 0, 0}) == doctest::Approx(-0.5f));
    CHECK(sdf::torus(2.0f, 0.5f)({0, 0, 0}) == doctest::Approx(1.5f));
    CHECK(sdf::capsule({0, -1, 0}, {0, 1, 0}, 0.5f)({0, 3, 0}) == doctest::Approx(1.5f));
    CHECK(sdf::cylinder(1.0f, 2.0f)({0, 3, 0}) == doctest::Approx(1.0f));
    CHECK(sdf::cylinder(1.0f, 2.0f)({3, 0, 0}) == doctest::Approx(2.0f));
    CHECK(sdf::plane({0, 2, 0}, -1.0f)({0, 4, 0}) == doctest::Approx(3.0f));
    CHECK(sdf::roundBox({1, 1, 1}, 0.25f)({0, 0, 0}) < 0.0f);
}

TEST_CASE("Booleanas: unión, resta e intersección") {
    auto a = sdf::sphere(1.0f);
    auto b = sdf::opTranslate(sdf::sphere(1.0f), {1.5f, 0, 0});
    auto u = sdf::opUnion(a, b);
    CHECK(u({0, 0, 0}) < 0.0f);
    CHECK(u({1.5f, 0, 0}) < 0.0f);
    auto sub = sdf::opSubtract(a, b);
    CHECK(sub({-0.5f, 0, 0}) < 0.0f);
    CHECK(sub({0.8f, 0, 0}) > 0.0f); // La parte compartida se quitó
    auto inter = sdf::opIntersect(a, b);
    CHECK(inter({0.75f, 0, 0}) < 0.0f);
    CHECK(inter({-0.5f, 0, 0}) > 0.0f);
}

TEST_CASE("Booleanas suaves: k = 0 es la dura; la unión suave nunca supera a la dura") {
    auto a = sdf::sphere(1.0f);
    auto b = sdf::opTranslate(sdf::box({0.5f, 0.5f, 0.5f}), {1.2f, 0, 0});
    for (float x = -2.0f; x <= 2.0f; x += 0.25f) {
        const glm::vec3 p(x, 0.3f, -0.2f);
        CHECK(sdf::opSmoothUnion(a, b, 0.0f)(p) == doctest::Approx(sdf::opUnion(a, b)(p)));
        CHECK(sdf::opSmoothUnion(a, b, 0.4f)(p) <= sdf::opUnion(a, b)(p) + 1e-6f);
        CHECK(sdf::opSmoothIntersect(a, b, 0.4f)(p) >= sdf::opIntersect(a, b)(p) - 1e-6f);
        CHECK(sdf::opSmoothSubtract(a, b, 0.4f)(p) >= sdf::opSubtract(a, b)(p) - 1e-6f);
    }
}

TEST_CASE("Transformaciones: traslación, rotación, escala, repetición, torsión") {
    auto moved = sdf::opTranslate(sdf::sphere(1.0f), {5, 0, 0});
    CHECK(moved({5, 0, 0}) == doctest::Approx(-1.0f));

    auto tall = sdf::box({0.5f, 2.0f, 0.5f});
    auto lying = sdf::opRotate(tall, glm::angleAxis(glm::radians(90.0f), glm::vec3(0, 0, 1)));
    CHECK(lying({1.9f, 0, 0}) < 0.0f); // La caja alta ahora está acostada sobre X
    CHECK(lying({0, 1.9f, 0}) > 0.0f);

    auto big = sdf::opScale(sdf::sphere(1.0f), 3.0f);
    CHECK(big({5, 0, 0}) == doctest::Approx(2.0f)); // Distancias correctas tras escalar

    auto repeated = sdf::opRepeat(sdf::sphere(0.3f), {2.0f, 0.0f, 2.0f});
    CHECK(repeated({0.1f, 0, 0.1f}) == doctest::Approx(repeated({4.1f, 0, -1.9f})));
    CHECK(repeated({0, 5, 0}) == doctest::Approx(4.7f)); // Y no se repite

    auto twisted = sdf::opTwist(sdf::box({1.0f, 2.0f, 0.2f}), 0.5f);
    CHECK(twisted({0, 0, 0}) < 0.0f);
    CHECK(sdf::opRound(sdf::sphere(1.0f), 0.5f)({1.4f, 0, 0}) < 0.0f);
}

TEST_CASE("normal() apunta hacia afuera") {
    glm::vec3 n = sdf::normal(sdf::sphere(1.0f), {0, 1, 0});
    CHECK(n.y == doctest::Approx(1.0f).epsilon(1e-3));
}

TEST_CASE("surfaceNets de una esfera: vértices sobre el radio, hermética, normales hacia afuera") {
    const int resolution = 32;
    MeshData mesh = sdf::surfaceNets(sdf::sphere(1.0f), kBounds, resolution);
    const float cell = 4.0f / resolution;
    CHECK(mesh.validate().empty());
    CHECK(mesh.triangleCount() > 100);
    for (const glm::vec3& p : mesh.positions) CHECK(std::abs(glm::length(p) - 1.0f) < cell);
    CHECK(isWatertight(mesh));
    CHECK(windingMatchesNormals(mesh));
    CHECK(normalsPointOutward(mesh));
}

TEST_CASE("surfaceNets: toro (género 1) y formas compuestas herméticos") {
    MeshData torus = sdf::surfaceNets(sdf::torus(1.0f, 0.35f), kBounds, 48);
    CHECK(isWatertight(torus));
    CHECK(windingMatchesNormals(torus));

    auto blob = sdf::opSmoothUnion(sdf::sphere(0.8f),
                                   sdf::opTranslate(sdf::box({0.5f, 0.5f, 0.5f}), {0.9f, 0.3f, 0}), 0.3f);
    MeshData mesh = sdf::surfaceNets(blob, kBounds, 40);
    CHECK(mesh.validate().empty());
    CHECK(isWatertight(mesh));
    CHECK(windingMatchesNormals(mesh));
}

TEST_CASE("surfaceNets: más resolución, más vértices; vacío si no hay superficie dentro") {
    auto shape = sdf::sphere(1.0f);
    CHECK(sdf::surfaceNets(shape, kBounds, 48).vertexCount() >
          sdf::surfaceNets(shape, kBounds, 16).vertexCount());
    CHECK(sdf::surfaceNets(sdf::sphere(0.1f), Aabb({5, 5, 5}, {6, 6, 6}), 8).empty()); // Todo afuera
    CHECK(sdf::surfaceNets(sdf::sphere(100.0f), kBounds, 8).empty());                  // Todo adentro
    CHECK(sdf::surfaceNets(shape, Aabb(), 8).empty());                                 // Caja vacía
}
