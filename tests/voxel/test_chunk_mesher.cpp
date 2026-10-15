#include "../procgen/MeshChecks.h"
#include "VoxelTestWorld.h"

#include <doctest/doctest.h>
#include <mge/voxel/ChunkMesher.h>

#include <memory>

using namespace mge;
using mge_test::TestBlocks;

TEST_CASE("Mesher: un bloque suelto es un cubo cerrado de 6 caras") {
    TestBlocks b;
    VoxelWorld world(b.registry);
    world.loadChunk({0, 0, 0});
    world.setBlock({3, 4, 5}, b.stone);

    const ChunkMesh mesh = buildChunkMesh(world, {0, 0, 0});
    CHECK(mesh.transparent.empty());
    CHECK(mesh.opaque.validate().empty());
    CHECK(mesh.opaque.triangleCount() == 12);
    CHECK(mge_test::isWatertight(mesh.opaque));
    CHECK(mge_test::windingMatchesNormals(mesh.opaque));
    for (std::size_t i = 0; i < mesh.opaque.vertexCount(); i++) // Hacia afuera del centro del cubo
        CHECK(glm::dot(mesh.opaque.normals[i], mesh.opaque.positions[i] - glm::vec3(3.5f, 4.5f, 5.5f)) >
              0.0f);
    const Aabb box = mesh.opaque.bounds();
    CHECK(box.min == glm::vec3(3, 4, 5));
    CHECK(box.max == glm::vec3(4, 5, 6));
    // Solo color: el color del bloque en cada vértice
    for (const glm::vec4& c : mesh.opaque.colors) CHECK(c == b.registry.get(b.stone).color);
}

TEST_CASE("Mesher: las caras tapadas entre bloques opacos no se emiten") {
    TestBlocks b;
    VoxelWorld world(b.registry);
    world.loadChunk({0, 0, 0});
    world.setBlock({0, 0, 0}, b.stone);
    world.setBlock({1, 0, 0}, b.stone);
    const ChunkMesh two = buildChunkMesh(world, {0, 0, 0});
    CHECK(two.opaque.triangleCount() == 2 * (12 - 2)); // 10 caras visibles
    CHECK(mge_test::isWatertight(two.opaque));

    // Bloque 3×3×3 macizo: solo la superficie (6 lados × 9 caras)
    for (int x = 4; x < 7; x++)
        for (int y = 4; y < 7; y++)
            for (int z = 4; z < 7; z++) world.setBlock({x, y, z}, b.stone);
    const ChunkMesh cube = buildChunkMesh(world, {0, 0, 0});
    CHECK(cube.opaque.triangleCount() == two.opaque.triangleCount() + 6 * 9 * 2);
}

TEST_CASE("Mesher: transparentes aparte; agua contra agua se oculta, contra vidrio no") {
    TestBlocks b;
    VoxelWorld world(b.registry);
    world.loadChunk({0, 0, 0});
    world.setBlock({0, 0, 0}, b.water);
    world.setBlock({1, 0, 0}, b.water);
    world.setBlock({2, 0, 0}, b.glass);
    world.setBlock({3, 0, 0}, b.stone);

    const ChunkMesh mesh = buildChunkMesh(world, {0, 0, 0});
    // Opaco: piedra con 6 caras (el vidrio no la tapa)
    CHECK(mesh.opaque.triangleCount() == 12);
    // Agua: 2 bloques × 6 caras − 2 (entre ellas) = 10. Vidrio: 6 − 1 (contra la piedra) = 5
    CHECK(mesh.transparent.triangleCount() == 2 * (10 + 5));
}

TEST_CASE("Mesher: los bloques en cruz son 4 quads (dos planos, dos caras)") {
    TestBlocks b;
    VoxelWorld world(b.registry);
    world.loadChunk({0, 0, 0});
    world.setBlock({2, 0, 2}, b.flower);
    world.setBlock({2, 0, 3}, b.stone); // La flor no oculta caras de la piedra
    const ChunkMesh mesh = buildChunkMesh(world, {0, 0, 0});
    CHECK(mesh.transparent.triangleCount() == 8);
    CHECK(mge_test::windingMatchesNormals(mesh.transparent));
    CHECK(mesh.opaque.triangleCount() == 12);
}

TEST_CASE("Mesher: los vecinos se leen del mundo (chunk de al lado cargado / sin cargar)") {
    TestBlocks b;
    VoxelWorld world(b.registry);
    world.loadChunk({0, 0, 0});
    world.setBlock({15, 0, 0}, b.stone);
    CHECK(buildChunkMesh(world, {0, 0, 0}).opaque.triangleCount() == 12); // Vecino sin cargar = aire

    world.loadChunk({1, 0, 0});
    world.setBlock({16, 0, 0}, b.stone); // Al otro lado del borde
    CHECK(buildChunkMesh(world, {0, 0, 0}).opaque.triangleCount() == 10);
    CHECK(buildChunkMesh(world, {1, 0, 0}).opaque.triangleCount() == 10);
}

TEST_CASE("Mesher: tiles del atlas por cara; chunk sin cargar da mallas vacías") {
    BlockRegistry registry;
    BlockDesc grass;
    grass.name = "grass";
    grass.setTiles(0, 2, 3);
    const BlockId id = registry.add(grass);
    VoxelWorld world(registry);
    world.loadChunk({0, 0, 0});
    world.setBlock({0, 0, 0}, id);

    ChunkMeshOptions options;
    options.atlas = {16, 16};
    const ChunkMesh mesh = buildChunkMesh(world, {0, 0, 0}, options);
    REQUIRE(mesh.opaque.vertexCount() == 24);
    // Cada vértice cae dentro del tile de su cara según la normal
    for (std::size_t i = 0; i < mesh.opaque.vertexCount(); i++) {
        const glm::vec3 n = mesh.opaque.normals[i];
        const int tile = n.y > 0.5f ? 0 : n.y < -0.5f ? 2 : 3;
        const UvRect r = options.atlas.tileUv(tile);
        const glm::vec2 uv = mesh.opaque.uvs[i];
        CHECK(uv.x >= r.min.x - 1e-6f);
        CHECK(uv.x <= r.max.x + 1e-6f);
        CHECK(uv.y >= r.min.y - 1e-6f);
        CHECK(uv.y <= r.max.y + 1e-6f);
    }

    CHECK(buildChunkMesh(world, {5, 0, 0}).opaque.empty());
}
