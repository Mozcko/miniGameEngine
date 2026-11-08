#include "VoxelTestWorld.h"

#include <doctest/doctest.h>
#include <glm/gtc/constants.hpp>
#include <mge/voxel/VoxelRaycast.h>

#include <cmath>
#include <limits>

using namespace mge;
using mge_test::TestBlocks;

TEST_CASE("raycastVoxels: impacto en las 6 caras (+X, -X, +Y, -Y, +Z, -Z)") {
    TestBlocks b;
    VoxelWorld world(b.registry);
    world.loadChunk({0, 0, 0});

    const glm::ivec3 targetBlock(5, 5, 5);
    world.setBlock(targetBlock, b.stone);

    SUBCASE("Desde -X hacia +X (impacta cara izquierda)") {
        const Ray ray({2.5f, 5.5f, 5.5f}, {1.0f, 0.0f, 0.0f});
        const auto hit = raycastVoxels(world, ray, 10.0f);
        REQUIRE(hit.has_value());
        CHECK(hit->block == targetBlock);
        CHECK(hit->normal == glm::ivec3(-1, 0, 0));
        CHECK(hit->faceNormal() == glm::vec3(-1.0f, 0.0f, 0.0f));
        CHECK(hit->face == BlockFace::Left);
        CHECK(hit->blockId == b.stone);
        CHECK(hit->distance == doctest::Approx(2.5f));
        CHECK(hit->point.x == doctest::Approx(5.0f));
        CHECK(hit->point.y == doctest::Approx(5.5f));
        CHECK(hit->point.z == doctest::Approx(5.5f));
    }

    SUBCASE("Desde +X hacia -X (impacta cara derecha)") {
        const Ray ray({7.5f, 5.5f, 5.5f}, {-1.0f, 0.0f, 0.0f});
        const auto hit = raycastVoxels(world, ray, 10.0f);
        REQUIRE(hit.has_value());
        CHECK(hit->block == targetBlock);
        CHECK(hit->normal == glm::ivec3(1, 0, 0));
        CHECK(hit->face == BlockFace::Right);
        CHECK(hit->distance == doctest::Approx(1.5f));
        CHECK(hit->point.x == doctest::Approx(6.0f));
    }

    SUBCASE("Desde -Y hacia +Y (impacta cara inferior)") {
        const Ray ray({5.5f, 2.0f, 5.5f}, {0.0f, 1.0f, 0.0f});
        const auto hit = raycastVoxels(world, ray, 10.0f);
        REQUIRE(hit.has_value());
        CHECK(hit->block == targetBlock);
        CHECK(hit->normal == glm::ivec3(0, -1, 0));
        CHECK(hit->face == BlockFace::Bottom);
        CHECK(hit->distance == doctest::Approx(3.0f));
        CHECK(hit->point.y == doctest::Approx(5.0f));
    }

    SUBCASE("Desde +Y hacia -Y (impacta cara superior)") {
        const Ray ray({5.5f, 9.5f, 5.5f}, {0.0f, -1.0f, 0.0f});
        const auto hit = raycastVoxels(world, ray, 10.0f);
        REQUIRE(hit.has_value());
        CHECK(hit->block == targetBlock);
        CHECK(hit->normal == glm::ivec3(0, 1, 0));
        CHECK(hit->face == BlockFace::Top);
        CHECK(hit->distance == doctest::Approx(3.5f));
        CHECK(hit->point.y == doctest::Approx(6.0f));
    }

    SUBCASE("Desde -Z hacia +Z (impacta cara trasera)") {
        const Ray ray({5.5f, 5.5f, 1.0f}, {0.0f, 0.0f, 1.0f});
        const auto hit = raycastVoxels(world, ray, 10.0f);
        REQUIRE(hit.has_value());
        CHECK(hit->block == targetBlock);
        CHECK(hit->normal == glm::ivec3(0, 0, -1));
        CHECK(hit->face == BlockFace::Back);
        CHECK(hit->distance == doctest::Approx(4.0f));
        CHECK(hit->point.z == doctest::Approx(5.0f));
    }

    SUBCASE("Desde +Z hacia -Z (impacta cara frontal)") {
        const Ray ray({5.5f, 5.5f, 8.2f}, {0.0f, 0.0f, -1.0f});
        const auto hit = raycastVoxels(world, ray, 10.0f);
        REQUIRE(hit.has_value());
        CHECK(hit->block == targetBlock);
        CHECK(hit->normal == glm::ivec3(0, 0, 1));
        CHECK(hit->face == BlockFace::Front);
        CHECK(hit->distance == doctest::Approx(2.2f));
        CHECK(hit->point.z == doctest::Approx(6.0f));
    }
}

TEST_CASE("raycastVoxels: cruza bordes de chunk positivos y negativos") {
    TestBlocks b;
    VoxelWorld world(b.registry);

    SUBCASE("Cruce a chunk vecino positivo (+X, de chunk 0 a chunk 1)") {
        world.loadChunk({0, 0, 0});
        world.loadChunk({1, 0, 0});

        // Bloque en x = 18 (chunk {1, 0, 0}, local x = 2)
        const glm::ivec3 target(18, 4, 4);
        world.setBlock(target, b.stone);

        // Rayo parte en chunk 0 en x = 14.5, cruza el borde x = 16 y llega a x = 18
        const Ray ray({14.5f, 4.5f, 4.5f}, {1.0f, 0.0f, 0.0f});
        const auto hit = raycastVoxels(world, ray, 10.0f);
        REQUIRE(hit.has_value());
        CHECK(hit->block == target);
        CHECK(hit->normal == glm::ivec3(-1, 0, 0));
        CHECK(hit->distance == doctest::Approx(3.5f));
    }

    SUBCASE("Cruce a chunk vecino negativo (-X, de chunk 0 a chunk -1)") {
        world.loadChunk({0, 0, 0});
        world.loadChunk({-1, 0, 0});

        // Bloque en x = -3 (chunk {-1, 0, 0}, local x = 13)
        const glm::ivec3 target(-3, 4, 4);
        world.setBlock(target, b.stone);

        // Rayo parte en chunk 0 en x = 1.5, cruza el borde x = 0 y llega a x = -2
        const Ray ray({1.5f, 4.5f, 4.5f}, {-1.0f, 0.0f, 0.0f});
        const auto hit = raycastVoxels(world, ray, 10.0f);
        REQUIRE(hit.has_value());
        CHECK(hit->block == target);
        CHECK(hit->normal == glm::ivec3(1, 0, 0));
        CHECK(hit->distance == doctest::Approx(3.5f));
    }

    SUBCASE("Cruce diagonal atravesando chunks negativos en X, Y, Z") {
        world.loadChunk({0, 0, 0});
        world.loadChunk({-1, -1, -1});

        const glm::ivec3 target(-2, -2, -2);
        world.setBlock(target, b.stone);

        const glm::vec3 start(1.5f, 1.5f, 1.5f);
        const Ray ray(start, glm::vec3(-1.0f)); // Se normaliza en el ctor de Ray
        const auto hit = raycastVoxels(world, ray, 20.0f);
        REQUIRE(hit.has_value());
        CHECK(hit->block == target);
        // Llega a la esquina superior derecha exterior (-1, -1, -1) del bloque objetivo
        CHECK(hit->distance > 0.0f);
    }
}

TEST_CASE("raycastVoxels: origen dentro de bloque sólido reporta impacto en t = 0") {
    TestBlocks b;
    VoxelWorld world(b.registry);
    world.loadChunk({0, 0, 0});

    const glm::ivec3 target(3, 3, 3);
    world.setBlock(target, b.stone);

    const Ray ray({3.4f, 3.8f, 3.1f}, {0.3f, 0.9f, 0.1f});
    const auto hit = raycastVoxels(world, ray, 5.0f);
    REQUIRE(hit.has_value());
    CHECK(hit->block == target);
    CHECK(hit->distance == 0.0f);
    CHECK(hit->normal == glm::ivec3(0));
    CHECK(hit->blockId == b.stone);
    CHECK(hit->point == ray.origin);
}

TEST_CASE("raycastVoxels: ignora bloques no sólidos (agua y flores) e impacta sólido") {
    TestBlocks b;
    VoxelWorld world(b.registry);
    world.loadChunk({0, 0, 0});

    world.setBlock({2, 1, 1}, b.water);
    world.setBlock({3, 1, 1}, b.flower);
    world.setBlock({4, 1, 1}, b.stone);

    const Ray ray({0.5f, 1.5f, 1.5f}, {1.0f, 0.0f, 0.0f});
    const auto hit = raycastVoxels(world, ray, 10.0f);
    REQUIRE(hit.has_value());
    CHECK(hit->block == glm::ivec3(4, 1, 1));
    CHECK(hit->distance == doctest::Approx(3.5f));
    CHECK(hit->normal == glm::ivec3(-1, 0, 0));
}

TEST_CASE("raycastVoxels: maxDistance limita el alcance del rayo") {
    TestBlocks b;
    VoxelWorld world(b.registry);
    world.loadChunk({0, 0, 0});

    world.setBlock({8, 0, 0}, b.stone);
    const Ray ray({0.5f, 0.5f, 0.5f}, {1.0f, 0.0f, 0.0f});

    CHECK_FALSE(raycastVoxels(world, ray, 5.0f).has_value());

    const auto hit = raycastVoxels(world, ray, 8.0f);
    REQUIRE(hit.has_value());
    CHECK(hit->block == glm::ivec3(8, 0, 0));
    CHECK(hit->distance == doctest::Approx(7.5f));
}

TEST_CASE("raycastVoxels: mundo vacío o rayo hacia el vacío devuelve nullopt") {
    TestBlocks b;
    VoxelWorld world(b.registry);
    world.loadChunk({0, 0, 0});

    const Ray ray({0.5f, 0.5f, 0.5f}, {0.0f, 1.0f, 0.0f});
    CHECK_FALSE(raycastVoxels(world, ray, 50.0f).has_value());
}

TEST_CASE("raycastVoxels: parámetros inválidos devuelven nullopt") {
    TestBlocks b;
    VoxelWorld world(b.registry);
    world.loadChunk({0, 0, 0});
    world.setBlock({2, 0, 0}, b.stone);

    const Ray ray({0.5f, 0.5f, 0.5f}, {1.0f, 0.0f, 0.0f});

    CHECK_FALSE(raycastVoxels(world, ray, -1.0f).has_value());
    CHECK_FALSE(raycastVoxels(world, ray, std::numeric_limits<float>::quiet_NaN()).has_value());
    CHECK_FALSE(raycastVoxels(world, ray, 0.0f).has_value());
}

TEST_CASE("raycastVoxels: desempate determinista en esquinas/aristas prefiere X sobre Y sobre Z") {
    TestBlocks b;
    VoxelWorld world(b.registry);
    world.loadChunk({0, 0, 0});
    world.loadChunk({1, 0, 0});

    // Bloques sólidos en (1, 0, 0) y (1, 1, 0)
    world.setBlock({1, 0, 0}, b.stone);
    world.setBlock({1, 1, 0}, b.stone);

    // Rayo desde el centro de (0,0,0) en diagonal exacta (1, 1, 0) hacia la arista
    const Ray ray({0.5f, 0.5f, 0.5f}, {1.0f, 1.0f, 0.0f});
    const auto hit = raycastVoxels(world, ray, 10.0f);
    REQUIRE(hit.has_value());
    // El desempate debe elegir X primero, impactando el bloque (1, 0, 0) en su cara izquierda (-X)
    CHECK(hit->block == glm::ivec3(1, 0, 0));
    CHECK(hit->normal == glm::ivec3(-1, 0, 0));
    CHECK(hit->face == BlockFace::Left);
}

TEST_CASE("raycastVoxels: maxDistance infinita no crashea y encuentra bloque a distancia 50") {
    TestBlocks b;
    VoxelWorld world(b.registry);
    for (int cx = 0; cx <= 3; ++cx) {
        world.loadChunk({cx, 0, 0});
    }

    world.setBlock({50, 0, 0}, b.stone);
    const Ray ray({0.5f, 0.5f, 0.5f}, {1.0f, 0.0f, 0.0f});

    const auto hit = raycastVoxels(world, ray, std::numeric_limits<float>::infinity());
    REQUIRE(hit.has_value());
    CHECK(hit->block == glm::ivec3(50, 0, 0));
    CHECK(hit->distance == doctest::Approx(49.5f));
}
