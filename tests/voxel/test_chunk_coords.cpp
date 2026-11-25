#include <doctest/doctest.h>
#include <mge/voxel/Chunk.h>

using namespace mge;

TEST_CASE("floorDiv / floorMod round toward -infinity") {
    CHECK(floorDiv(0, 16) == 0);
    CHECK(floorDiv(15, 16) == 0);
    CHECK(floorDiv(16, 16) == 1);
    CHECK(floorDiv(-1, 16) == -1);
    CHECK(floorDiv(-16, 16) == -1);
    CHECK(floorDiv(-17, 16) == -2);
    CHECK(floorMod(-1, 16) == 15);
    CHECK(floorMod(-16, 16) == 0);
    CHECK(floorMod(-17, 16) == 15);
    CHECK(floorMod(33, 16) == 1);
    // Identity for all a: a == floorDiv * b + floorMod, with 0 <= floorMod < b
    for (int a = -100; a <= 100; a++) {
        CAPTURE(a);
        CHECK(floorDiv(a, 16) * 16 + floorMod(a, 16) == a);
        CHECK(floorMod(a, 16) >= 0);
        CHECK(floorMod(a, 16) < 16);
    }
}

TEST_CASE("World -> chunk -> local, including negative and vertical coordinates") {
    CHECK(chunkOf({0, 0, 0}) == ChunkCoord{0, 0, 0});
    CHECK(chunkOf({-1, 16, 31}) == ChunkCoord{-1, 1, 1});
    CHECK(localOf({-1, 16, 31}) == glm::ivec3(15, 0, 15));
    CHECK(chunkOrigin({-1, 1, 1}) == glm::ivec3(-16, 16, 16));
    for (const glm::ivec3 p : {glm::ivec3(5, -3, 40), glm::ivec3(-33, 100, -16), glm::ivec3(0, -1, 15)}) {
        CHECK(chunkOrigin(chunkOf(p)) + localOf(p) == p);
    }
}

TEST_CASE("blockAt: block x spans from x to x+1 (exclusive)") {
    CHECK(blockAt({0.0f, 0.0f, 0.0f}) == glm::ivec3(0, 0, 0));
    CHECK(blockAt({0.99f, 1.5f, 2.0f}) == glm::ivec3(0, 1, 2));
    CHECK(blockAt({-0.01f, -1.0f, -1.5f}) == glm::ivec3(-1, -1, -2));
}

TEST_CASE("ChunkData: starts empty, get/set, out-of-range returns air") {
    ChunkData chunk;
    CHECK(chunk.isEmpty());
    chunk.set(1, 2, 3, 7);
    CHECK(chunk.get(1, 2, 3) == 7);
    CHECK(chunk.get(glm::ivec3(1, 2, 3)) == 7);
    CHECK_FALSE(chunk.isEmpty());
    CHECK(chunk.get(-1, 0, 0) == kAir);
    CHECK(chunk.get(0, kChunkSize, 0) == kAir);
    chunk.set(kChunkSize, 0, 0, 9); // Ignored
    chunk.fill(kAir);
    CHECK(chunk.isEmpty());
    // Unique indices within array bounds
    CHECK(ChunkData::index(0, 0, 0) == 0);
    CHECK(ChunkData::index(15, 15, 15) == kChunkVolume - 1);
}

TEST_CASE("ChunkCoordHash distinguishes neighboring coordinates") {
    ChunkCoordHash hash;
    CHECK(hash({0, 0, 0}) != hash({1, 0, 0}));
    CHECK(hash({1, 0, 0}) != hash({0, 1, 0}));
    CHECK(hash({0, 1, 0}) != hash({0, 0, 1}));
    CHECK(hash({-1, 0, 0}) != hash({1, 0, 0}));
}
