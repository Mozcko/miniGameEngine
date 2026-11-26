#include <doctest/doctest.h>

#include <mge/physics/MeshCollider.h>
#include <mge/procgen/Primitives.h>

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

using namespace mge;

TEST_CASE("MeshCollider: raycast hits exact triangle and returns nearest hit") {
    MeshCollider collider;
    // Two stacked horizontal quads at y = 0 and y = 4
    collider.addTriangle({-2.0f, 0.0f, -2.0f}, {-2.0f, 0.0f, 2.0f}, {2.0f, 0.0f, 2.0f});
    collider.addTriangle({-2.0f, 0.0f, -2.0f}, {2.0f, 0.0f, 2.0f}, {2.0f, 0.0f, -2.0f});
    collider.addTriangle({-2.0f, 4.0f, -2.0f}, {-2.0f, 4.0f, 2.0f}, {2.0f, 4.0f, 2.0f});
    collider.addTriangle({-2.0f, 4.0f, -2.0f}, {2.0f, 4.0f, 2.0f}, {2.0f, 4.0f, -2.0f});
    collider.build();

    // Downward ray from y = 10 should hit y = 4 first (distance 6)
    const auto hitDown = collider.raycast(Ray({0.0f, 10.0f, 0.0f}, {0.0f, -1.0f, 0.0f}), 20.0f);
    REQUIRE(hitDown.has_value());
    CHECK(hitDown->distance == doctest::Approx(6.0f));
    CHECK(hitDown->point.y == doctest::Approx(4.0f));
    CHECK(hitDown->normal.y == doctest::Approx(1.0f));

    // Upward ray from y = -5 should hit y = 0 first (distance 5) with normal oriented towards origin (-Y)
    const auto hitUp = collider.raycast(Ray({0.0f, -5.0f, 0.0f}, {0.0f, 1.0f, 0.0f}), 20.0f);
    REQUIRE(hitUp.has_value());
    CHECK(hitUp->distance == doctest::Approx(5.0f));
    CHECK(hitUp->point.y == doctest::Approx(0.0f));
    CHECK(hitUp->normal.y == doctest::Approx(-1.0f));

    // Max distance cutoff
    CHECK_FALSE(collider.raycast(Ray({0.0f, 10.0f, 0.0f}, {0.0f, -1.0f, 0.0f}), 5.0f).has_value());

    // Ray missing the quads
    CHECK_FALSE(collider.raycast(Ray({10.0f, 10.0f, 0.0f}, {0.0f, -1.0f, 0.0f}), 50.0f).has_value());
}

TEST_CASE(
    "MeshCollider: high-speed capsuleCast and sphereCast stop before wall without tunnelling (100 m/s)") {
    MeshCollider wall;
    // Thin vertical wall at z = 0 spanning x in [-10, 10], y in [-10, 10]
    wall.addTriangle({-10.0f, -10.0f, 0.0f}, {10.0f, -10.0f, 0.0f}, {10.0f, 10.0f, 0.0f});
    wall.addTriangle({-10.0f, -10.0f, 0.0f}, {10.0f, 10.0f, 0.0f}, {-10.0f, 10.0f, 0.0f});
    wall.build();

    // Capsule at z = 5 moving at -100 units in one step (100 m/s * 1 s)
    const Capsule cap{{0.0f, 0.0f, 5.0f}, 0.4f, 1.8f};
    const glm::vec3 delta(0.0f, 0.0f, -100.0f);
    const ShapeCastHit capHit = wall.capsuleCast(cap, delta, 1e-3f);
    REQUIRE(capHit.hit);
    const float capEndZ = cap.feet.z + delta.z * capHit.fraction;
    CHECK(capEndZ >= cap.radius - 0.01f);
    CHECK(capEndZ <= cap.radius + 0.05f);
    CHECK(capHit.normal.z == doctest::Approx(1.0f));

    // Sphere at z = 5 moving at -100 units in one step
    const Sphere sphere{{0.0f, 1.0f, 5.0f}, 0.5f};
    const ShapeCastHit sphereHit = wall.sphereCast(sphere, delta, 1e-3f);
    REQUIRE(sphereHit.hit);
    const float sphereEndZ = sphere.center.z + delta.z * sphereHit.fraction;
    CHECK(sphereEndZ >= sphere.radius - 0.01f);
    CHECK(sphereEndZ <= sphere.radius + 0.05f);
    CHECK(sphereHit.normal.z == doctest::Approx(1.0f));
}

TEST_CASE("MeshCollider: overlapSphere and overlapCapsule report contacts and clear separation") {
    const MeshData box = primitives::box(glm::vec3(2.0f)); // [-1, 1]^3
    const MeshCollider collider = MeshCollider::fromMesh(box);
    REQUIRE(collider.triangleCount() == 12);

    std::vector<ShapeContact> contacts;
    // Sphere penetrating +Y face (y = 1)
    CHECK(collider.overlapSphere(Sphere{{0.0f, 1.25f, 0.0f}, 0.5f}, contacts));
    REQUIRE_FALSE(contacts.empty());
    CHECK(contacts[0].penetration == doctest::Approx(0.25f).epsilon(1e-3f));

    // Separated sphere
    CHECK_FALSE(collider.overlapSphere(Sphere{{0.0f, 5.0f, 0.0f}, 0.5f}, contacts));
    CHECK(contacts.empty());

    // Capsule penetrating +X face (x = 1)
    CHECK(collider.overlapCapsule(Capsule{{1.2f, -0.5f, 0.0f}, 0.4f, 1.6f}, contacts));
    REQUIRE_FALSE(contacts.empty());

    // Separated capsule
    CHECK_FALSE(collider.overlapCapsule(Capsule{{5.0f, 0.0f, 0.0f}, 0.4f, 1.6f}, contacts));
    CHECK(contacts.empty());
}

TEST_CASE("MeshCollider: BVH queryAabb and raycast agree with linear scan on transformed grid") {
    MeshCollider bvh;
    MeshCollider linear;
    const MeshData unitBox = primitives::box(glm::vec3(1.0f));
    for (int gz = 0; gz < 6; ++gz) {
        for (int gx = 0; gx < 6; ++gx) {
            const glm::mat4 xf =
                glm::translate(glm::mat4(1.0f),
                               glm::vec3(static_cast<float>(gx) * 2.0f, 0.0f, static_cast<float>(gz) * 2.0f));
            bvh.addMesh(unitBox, xf);
            linear.addMesh(unitBox, xf);
        }
    }
    bvh.build(); // linear intentionally keeps empty BVH (linear fallback)

    const Aabb query({1.5f, -1.0f, 1.5f}, {4.5f, 1.0f, 4.5f});
    std::vector<std::uint32_t> bvhIds;
    std::vector<std::uint32_t> linearIds;
    bvh.queryAabb(query, bvhIds);
    linear.queryAabb(query, linearIds);
    std::sort(bvhIds.begin(), bvhIds.end());
    std::sort(linearIds.begin(), linearIds.end());
    CHECK(bvhIds == linearIds);

    const Ray ray({4.0f, 5.0f, 6.0f}, {0.0f, -1.0f, 0.0f});
    const auto hBvh = bvh.raycast(ray, 20.0f);
    const auto hLin = linear.raycast(ray, 20.0f);
    REQUIRE(hBvh.has_value());
    REQUIRE(hLin.has_value());
    CHECK(hBvh->distance == doctest::Approx(hLin->distance));
}
