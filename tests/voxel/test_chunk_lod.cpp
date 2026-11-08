#include "VoxelTestWorld.h"

#include <doctest/doctest.h>
#include <mge/voxel/ChunkLod.h>
#include <mge/voxel/ChunkSnapshot.h>
#include <mge/voxel/VoxelWorld.h>

using namespace mge;
using namespace mge_test;

TEST_CASE("voxel:ChunkLod: submuestreo de chunk lleno, vacio y semi-lleno") {
    TestBlocks tb;
    VoxelWorld world(tb.registry);

    // 1. Chunk vacío (todo aire)
    world.loadChunk({0, 0, 0});
    const ChunkSnapshot emptySnap = ChunkSnapshot::capture(world, {0, 0, 0});
    REQUIRE(emptySnap.valid());

    LodChunk emptyLod1 = downsample(emptySnap, tb.registry, 1);
    CHECK(emptyLod1.factor == 2);
    CHECK(emptyLod1.dimension == 8);
    for (int y = 0; y < 8; ++y) {
        for (int z = 0; z < 8; ++z) {
            for (int x = 0; x < 8; ++x) {
                CHECK(emptyLod1.get(x, y, z) == kAir);
            }
        }
    }

    LodChunk emptyLod2 = downsample(emptySnap, tb.registry, 2);
    CHECK(emptyLod2.factor == 4);
    CHECK(emptyLod2.dimension == 4);
    for (int y = 0; y < 4; ++y) {
        for (int z = 0; z < 4; ++z) {
            for (int x = 0; x < 4; ++x) {
                CHECK(emptyLod2.get(x, y, z) == kAir);
            }
        }
    }

    // 2. Chunk lleno (todo piedra)
    for (int y = 0; y < kChunkSize; ++y) {
        for (int z = 0; z < kChunkSize; ++z) {
            for (int x = 0; x < kChunkSize; ++x) {
                world.setBlock({x, y, z}, tb.stone);
            }
        }
    }
    const ChunkSnapshot fullSnap = ChunkSnapshot::capture(world, {0, 0, 0});
    LodChunk fullLod1 = downsample(fullSnap, tb.registry, 1);
    CHECK(fullLod1.dimension == 8);
    for (int y = 0; y < 8; ++y) {
        for (int z = 0; z < 8; ++z) {
            for (int x = 0; x < 8; ++x) {
                CHECK(fullLod1.get(x, y, z) == tb.stone);
            }
        }
    }

    LodChunk fullLod2 = downsample(fullSnap, tb.registry, 2);
    CHECK(fullLod2.dimension == 4);
    for (int y = 0; y < 4; ++y) {
        for (int z = 0; z < 4; ++z) {
            for (int x = 0; x < 4; ++x) {
                CHECK(fullLod2.get(x, y, z) == tb.stone);
            }
        }
    }

    // 3. Chunk semi-lleno (suelo plano hasta y = 8)
    VoxelWorld flatWorld(tb.registry, std::make_unique<FlatGenerator>(tb.stone, 8));
    flatWorld.loadChunk({0, 0, 0});
    const ChunkSnapshot halfSnap = ChunkSnapshot::capture(flatWorld, {0, 0, 0});

    LodChunk halfLod1 = downsample(halfSnap, tb.registry, 1);
    CHECK(halfLod1.dimension == 8);
    for (int y = 0; y < 8; ++y) {
        for (int z = 0; z < 8; ++z) {
            for (int x = 0; x < 8; ++x) {
                if (y < 4) {
                    CHECK(halfLod1.get(x, y, z) == tb.stone);
                } else {
                    CHECK(halfLod1.get(x, y, z) == kAir);
                }
            }
        }
    }

    LodChunk halfLod2 = downsample(halfSnap, tb.registry, 2);
    CHECK(halfLod2.dimension == 4);
    for (int y = 0; y < 4; ++y) {
        for (int z = 0; z < 4; ++z) {
            for (int x = 0; x < 4; ++x) {
                if (y < 2) {
                    CHECK(halfLod2.get(x, y, z) == tb.stone);
                } else {
                    CHECK(halfLod2.get(x, y, z) == kAir);
                }
            }
        }
    }
}

TEST_CASE("voxel:ChunkLod: regla del bloque opaco superior") {
    TestBlocks tb;
    BlockDesc grassDesc;
    grassDesc.name = "grass";
    grassDesc.opaque = true;
    grassDesc.color = {0.2f, 0.8f, 0.2f, 1.0f};
    const BlockId grass = tb.registry.add(grassDesc);

    VoxelWorld world(tb.registry);
    world.loadChunk({0, 0, 0});

    // Celda 1 en (0, 0, 0) con factor 2 (x in [0, 1], y in [0, 1], z in [0, 1]):
    // Capa y = 1: agua (translúcida) en (0, 1, 0), aire en el resto.
    // Capa y = 0: piedra (opaca) en (0, 0, 0).
    // La regla debe descartar el agua por no ser opaca y tomar la piedra.
    world.setBlock({0, 1, 0}, tb.water);
    world.setBlock({0, 0, 0}, tb.stone);

    // Celda 2 en (2, 0, 0) (sx = 1, sy = 0, sz = 0):
    // Capa y = 1: flor (recortada / shape Cross) en (2, 1, 0).
    // Capa y = 0: piedra en (2, 0, 0).
    // La regla debe descartar la flor y tomar la piedra.
    world.setBlock({2, 1, 0}, tb.flower);
    world.setBlock({2, 0, 0}, tb.stone);

    // Celda 3 en (4, 0, 0) (sx = 2, sy = 0, sz = 0):
    // Capa y = 1: pasto (opaco) en (4, 1, 0).
    // Capa y = 0: piedra (opaca) en (4, 0, 0).
    // Debe tomar el pasto porque está arriba de la piedra.
    world.setBlock({4, 1, 0}, grass);
    world.setBlock({4, 0, 0}, tb.stone);

    // Celda 4 en (6, 0, 0) (sx = 3, sy = 0, sz = 0):
    // Solo vidrio y agua (ninguno opaco).
    // Debe devolver aire.
    world.setBlock({6, 1, 0}, tb.glass);
    world.setBlock({6, 0, 0}, tb.water);

    const ChunkSnapshot snap = ChunkSnapshot::capture(world, {0, 0, 0});
    LodChunk lod = downsample(snap, tb.registry, 1);

    CHECK(lod.get(0, 0, 0) == tb.stone);
    CHECK(lod.get(1, 0, 0) == tb.stone);
    CHECK(lod.get(2, 0, 0) == grass);
    CHECK(lod.get(3, 0, 0) == kAir);
}

TEST_CASE("voxel:ChunkLod: conteo de triangulos de LOD 1 <= 1/4 de LOD 0 (+ faldas)") {
    TestBlocks tb;
    VoxelWorld world(tb.registry, std::make_unique<FlatGenerator>(tb.stone, 8));
    world.loadChunk({0, 0, 0});
    const ChunkSnapshot snap = ChunkSnapshot::capture(world, {0, 0, 0});

    ChunkMeshOptions optNaive;
    optNaive.greedy = false;
    optNaive.lodSkirts = false;

    // Sin greedy y sin faldas: LOD 1 tiene 8x8 bloques en vez de 16x16, el área superficial
    // en cada dirección se reduce exactamente por 4
    const ChunkMesh meshLod0 = buildLodChunkMesh(snap, tb.registry, 0, optNaive);
    const ChunkMesh meshLod1NoSkirts = buildLodChunkMesh(snap, tb.registry, 1, optNaive);

    const std::size_t tri0 = meshLod0.opaque.triangleCount();
    const std::size_t tri1NoSkirts = meshLod1NoSkirts.opaque.triangleCount();
    REQUIRE(tri0 > 0);
    CHECK(tri1NoSkirts <= tri0 / 4);

    // Con faldas en los 4 bordes: cada borde de 8 bloques añade 8 quads (16 triángulos) = 64 triángulos
    ChunkMeshOptions optWithSkirts = optNaive;
    optWithSkirts.lodSkirts = true;
    const ChunkMesh meshLod1WithSkirts = buildLodChunkMesh(snap, tb.registry, 1, optWithSkirts);
    const std::size_t tri1WithSkirts = meshLod1WithSkirts.opaque.triangleCount();
    CHECK(tri1WithSkirts <= tri0 / 4 + 64);
    CHECK(tri1WithSkirts > tri1NoSkirts);

    // Con greedy meshing
    ChunkMeshOptions optGreedy;
    optGreedy.greedy = true;
    optGreedy.lodSkirts = true;
    const ChunkMesh meshGreedy = buildLodChunkMesh(snap, tb.registry, 1, optGreedy);
    CHECK(meshGreedy.opaque.triangleCount() <= tri1WithSkirts);
}

TEST_CASE("voxel:ChunkLod: validacion de malla (validate pasa sin errores)") {
    TestBlocks tb;
    VoxelWorld world(tb.registry, std::make_unique<FlatGenerator>(tb.stone, 8));
    world.loadChunk({0, 0, 0});

    // Agregar algunos bloques variados
    world.setBlock({3, 9, 3}, tb.stone);
    world.setBlock({4, 9, 3}, tb.stone);
    world.setBlock({5, 8, 5}, tb.water); // agua genera cara superior transparente en LOD > 0

    const ChunkSnapshot snap = ChunkSnapshot::capture(world, {0, 0, 0});

    // LOD 0
    const ChunkMesh mesh0 = buildLodChunkMesh(snap, tb.registry, 0);
    CHECK(mesh0.opaque.validate().empty());

    // LOD 1
    const ChunkMesh mesh1 = buildLodChunkMesh(snap, tb.registry, 1);
    CHECK(mesh1.opaque.validate().empty());
    CHECK(mesh1.cutout.empty());
    CHECK(mesh1.transparent.validate().empty());

    // LOD 2
    const ChunkMesh mesh2 = buildLodChunkMesh(snap, tb.registry, 2);
    CHECK(mesh2.opaque.validate().empty());
    CHECK(mesh2.cutout.empty());
    CHECK(mesh2.transparent.validate().empty());
}

TEST_CASE("voxel:ChunkLod: costuras: dos chunks vecinos al mismo LOD no tienen caras internas") {
    TestBlocks tb;
    VoxelWorld world(tb.registry, std::make_unique<FlatGenerator>(tb.stone, 8));
    world.loadChunk({0, 0, 0});
    world.loadChunk({1, 0, 0}); // Vecino en +X

    const ChunkSnapshot snapA = ChunkSnapshot::capture(world, {0, 0, 0});
    const ChunkSnapshot snapB = ChunkSnapshot::capture(world, {1, 0, 0});

    ChunkMeshOptions options;
    options.greedy = true;
    options.lodSkirts = true;

    const ChunkMesh meshA = buildLodChunkMesh(snapA, tb.registry, 1, options);
    const ChunkMesh meshB = buildLodChunkMesh(snapB, tb.registry, 1, options);

    // En el chunk A, el límite con B es x = 16. No debe haber vértices ni caras orientadas hacia +X en x = 16
    for (std::size_t i = 0; i < meshA.opaque.positions.size(); ++i) {
        const glm::vec3& pos = meshA.opaque.positions[i];
        const glm::vec3& norm = meshA.opaque.normals[i];
        if (norm.x > 0.9f) {
            // No debe haber cara hacia +X en la interfaz compartida
            CHECK(pos.x < 15.99f);
        }
    }

    // En el chunk B, el límite con A es x = 0. No debe haber caras orientadas hacia -X en x = 0
    for (std::size_t i = 0; i < meshB.opaque.positions.size(); ++i) {
        const glm::vec3& pos = meshB.opaque.positions[i];
        const glm::vec3& norm = meshB.opaque.normals[i];
        if (norm.x < -0.9f) {
            // No debe haber cara hacia -X en la interfaz compartida
            CHECK(pos.x > 0.01f);
        }
    }
}

TEST_CASE("voxel:ChunkLod: faldas presentes en los bordes expuestos") {
    TestBlocks tb;
    VoxelWorld world(tb.registry, std::make_unique<FlatGenerator>(tb.stone, 8));
    world.loadChunk({0, 0, 0}); // Chunk aislado: sus 4 bordes laterales dan al aire

    const ChunkSnapshot snap = ChunkSnapshot::capture(world, {0, 0, 0});

    // Con faldas (factor 2): debe haber vértices que bajen hasta y = -2
    ChunkMeshOptions optWithSkirts;
    optWithSkirts.lodSkirts = true;
    const ChunkMesh meshWithSkirts = buildLodChunkMesh(snap, tb.registry, 1, optWithSkirts);

    bool hasSkirtVertices = false;
    for (const auto& pos : meshWithSkirts.opaque.positions) {
        if (pos.y < -0.01f) {
            hasSkirtVertices = true;
            CHECK(pos.y >= -2.01f);
        }
    }
    CHECK(hasSkirtVertices);

    // Sin faldas: no debe haber ningún vértice con y < 0
    ChunkMeshOptions optNoSkirts;
    optNoSkirts.lodSkirts = false;
    const ChunkMesh meshNoSkirts = buildLodChunkMesh(snap, tb.registry, 1, optNoSkirts);

    for (const auto& pos : meshNoSkirts.opaque.positions) {
        CHECK(pos.y >= -0.01f);
    }
}

TEST_CASE("voxel:ChunkLod: agua en LOD > 0 genera malla transparente en la superficie") {
    TestBlocks tb;
    VoxelWorld world(tb.registry);
    world.loadChunk({0, 0, 0});

    // Piedra hasta y = 4, agua en y = 5..7
    for (int y = 0; y <= 4; ++y) {
        for (int z = 0; z < kChunkSize; ++z) {
            for (int x = 0; x < kChunkSize; ++x) {
                world.setBlock({x, y, z}, tb.stone);
            }
        }
    }
    for (int y = 5; y <= 7; ++y) {
        for (int z = 0; z < kChunkSize; ++z) {
            for (int x = 0; x < kChunkSize; ++x) {
                world.setBlock({x, y, z}, tb.water);
            }
        }
    }

    const ChunkSnapshot snap = ChunkSnapshot::capture(world, {0, 0, 0});
    REQUIRE(snap.valid());

    const ChunkMesh meshLod1 = buildLodChunkMesh(snap, tb.registry, 1);
    CHECK_FALSE(meshLod1.transparent.empty());
    CHECK(meshLod1.transparent.vertexCount() > 0);

    for (const auto& pos : meshLod1.transparent.positions) {
        CHECK(pos.y == doctest::Approx(8.0f));
    }
}
