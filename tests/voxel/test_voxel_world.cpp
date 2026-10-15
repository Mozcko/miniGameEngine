#include "VoxelTestWorld.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <memory>

using namespace mge;
using mge_test::FlatGenerator;
using mge_test::TestBlocks;

TEST_CASE("VoxelWorld: chunks sin cargar son aire; loadChunk genera una sola vez") {
    TestBlocks b;
    VoxelWorld world(b.registry, std::make_unique<FlatGenerator>(b.stone, 4));
    CHECK(world.getBlock(0, 0, 0) == kAir);
    CHECK_FALSE(world.isLoaded({0, 0, 0}));

    ChunkData& chunk = world.loadChunk({0, 0, 0});
    CHECK(world.isLoaded({0, 0, 0}));
    CHECK(world.chunkCount() == 1);
    CHECK(world.getBlock(0, 3, 0) == b.stone);
    CHECK(world.getBlock(0, 4, 0) == kAir);

    chunk.set(0, 0, 0, b.glass);
    world.loadChunk({0, 0, 0}); // Ya existe: no se vuelve a generar
    CHECK(world.getBlock(0, 0, 0) == b.glass);

    world.unloadChunk({0, 0, 0});
    CHECK(world.getBlock(0, 3, 0) == kAir);
    CHECK(world.chunkCount() == 0);
}

TEST_CASE("VoxelWorld: get/set entre chunks, con coordenadas negativas y verticales") {
    TestBlocks b;
    VoxelWorld world(b.registry);
    for (int x = -1; x <= 0; x++)
        for (int y = -1; y <= 1; y++)
            for (int z = -1; z <= 0; z++) world.loadChunk({x, y, z});

    const glm::ivec3 points[] = {{-1, -1, -1}, {0, 0, 0}, {-16, 17, -5}, {15, -16, 0}, {-7, 31, -16}};
    for (const glm::ivec3& p : points) {
        CAPTURE(p.x);
        CAPTURE(p.y);
        CAPTURE(p.z);
        CHECK(world.setBlock(p, b.stone));
        CHECK(world.getBlock(p) == b.stone);
        CHECK(world.chunk(chunkOf(p))->get(localOf(p)) == b.stone);
    }
    CHECK(world.isSolid({0, 0, 0}));
    CHECK_FALSE(world.isSolid({1, 0, 0}));
    CHECK_FALSE(world.setBlock({100, 0, 0}, b.stone)); // Chunk sin cargar
}

TEST_CASE("VoxelWorld: marcas de sucio y propagación al vecino del borde") {
    TestBlocks b;
    VoxelWorld world(b.registry);
    for (int x = -1; x <= 1; x++) world.loadChunk({x, 0, 0});
    CHECK(world.dirtyChunks().size() == 3); // Recién cargados: sucios
    for (const ChunkCoord& c : world.loadedChunks()) world.clearDirty(c);
    world.loadChunk({2, 0, 0}); // Cargar un chunk ensucia a sus vecinos cargados (solo los de cara)
    CHECK(world.isDirty({2, 0, 0}));
    CHECK(world.isDirty({1, 0, 0}));
    CHECK_FALSE(world.isDirty({0, 0, 0}));
    world.unloadChunk({2, 0, 0});
    for (const ChunkCoord& c : world.loadedChunks()) world.clearDirty(c);
    CHECK(world.dirtyChunks().empty());

    world.setBlock({5, 5, 5}, b.stone); // Interior
    CHECK(world.isDirty({0, 0, 0}));
    CHECK_FALSE(world.isDirty({-1, 0, 0}));
    CHECK_FALSE(world.isDirty({1, 0, 0}));
    world.clearDirty({0, 0, 0});

    world.setBlock({0, 5, 5}, b.stone); // Borde -X: también el chunk de la izquierda
    CHECK(world.isDirty({0, 0, 0}));
    CHECK(world.isDirty({-1, 0, 0}));
    CHECK_FALSE(world.isDirty({1, 0, 0}));
    for (const ChunkCoord& c : world.loadedChunks()) world.clearDirty(c);

    world.setBlock({15, 0, 15}, b.stone); // Borde +X (+Y y +Z no cargados: se ignoran)
    const auto dirty = world.dirtyChunks();
    CHECK(dirty.size() == 2);
    CHECK(std::find(dirty.begin(), dirty.end(), ChunkCoord{1, 0, 0}) != dirty.end());

    world.markDirty({50, 0, 0}); // Sin cargar: no hace nada
    CHECK_FALSE(world.isDirty({50, 0, 0}));
}

TEST_CASE("VoxelWorld: el generador es determinista e independiente del orden de carga") {
    TestBlocks b;
    VoxelWorld a(b.registry, std::make_unique<FlatGenerator>(b.stone, 20));
    VoxelWorld c(b.registry, std::make_unique<FlatGenerator>(b.stone, 20));
    a.loadChunk({0, 0, 0});
    a.loadChunk({0, 1, 0});
    c.loadChunk({0, 1, 0});
    c.loadChunk({0, 0, 0});
    CHECK(a.chunk({0, 0, 0})->blocks() == c.chunk({0, 0, 0})->blocks());
    CHECK(a.chunk({0, 1, 0})->blocks() == c.chunk({0, 1, 0})->blocks());
    CHECK(a.getBlock(0, 19, 0) == b.stone);
    CHECK(a.getBlock(0, 20, 0) == kAir);
}
