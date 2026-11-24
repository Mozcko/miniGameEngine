#include "../procgen/MeshChecks.h"
#include "VoxelTestWorld.h"

#include <doctest/doctest.h>
#include <mge/voxel/ChunkMesher.h>

#include <array>
#include <cmath>
#include <memory>

#include <mge/core/Random.h>

using namespace mge;
using mge_test::TestBlocks;

namespace {
    // One face per block (no merging): counts match "naive" meshing
    ChunkMeshOptions naive() {
        ChunkMeshOptions options;
        options.greedy = false;
        return options;
    }
}

TEST_CASE("Mesher: a single isolated block is a closed 6-face cube") {
    TestBlocks b;
    VoxelWorld world(b.registry);
    world.loadChunk({0, 0, 0});
    world.setBlock({3, 4, 5}, b.stone);

    const ChunkMesh mesh = buildChunkMesh(world, {0, 0, 0}, naive());
    CHECK(mesh.transparent.empty());
    CHECK(mesh.opaque.validate().empty());
    CHECK(mesh.opaque.triangleCount() == 12);
    CHECK(mge_test::isWatertight(mesh.opaque));
    CHECK(mge_test::windingMatchesNormals(mesh.opaque));
    for (std::size_t i = 0; i < mesh.opaque.vertexCount(); i++) // Outward from the cube center
        CHECK(glm::dot(mesh.opaque.normals[i], mesh.opaque.positions[i] - glm::vec3(3.5f, 4.5f, 5.5f)) >
              0.0f);
    const Aabb box = mesh.opaque.bounds();
    CHECK(box.min == glm::vec3(3, 4, 5));
    CHECK(box.max == glm::vec3(4, 5, 6));
    // Color-only: the block's color at every vertex
    for (const glm::vec4& c : mesh.opaque.colors) CHECK(c == b.registry.get(b.stone).color);
}

TEST_CASE("Mesher: hidden faces between opaque blocks are not emitted") {
    TestBlocks b;
    VoxelWorld world(b.registry);
    world.loadChunk({0, 0, 0});
    world.setBlock({0, 0, 0}, b.stone);
    world.setBlock({1, 0, 0}, b.stone);
    const ChunkMesh two = buildChunkMesh(world, {0, 0, 0}, naive());
    CHECK(two.opaque.triangleCount() == 2 * (12 - 2)); // 10 visible faces
    CHECK(mge_test::isWatertight(two.opaque));

    // Solid 3x3x3 block: only the outer surface (6 sides x 9 faces)
    for (int x = 4; x < 7; x++)
        for (int y = 4; y < 7; y++)
            for (int z = 4; z < 7; z++) world.setBlock({x, y, z}, b.stone);
    const ChunkMesh cube = buildChunkMesh(world, {0, 0, 0}, naive());
    CHECK(cube.opaque.triangleCount() == two.opaque.triangleCount() + 6 * 9 * 2);
}

TEST_CASE("Mesher: transparent blocks separated; water-against-water is culled, against glass is not") {
    TestBlocks b;
    VoxelWorld world(b.registry);
    world.loadChunk({0, 0, 0});
    world.setBlock({0, 0, 0}, b.water);
    world.setBlock({1, 0, 0}, b.water);
    world.setBlock({2, 0, 0}, b.glass);
    world.setBlock({3, 0, 0}, b.stone);

    const ChunkMesh mesh = buildChunkMesh(world, {0, 0, 0}, naive());
    // Opaque: stone with 6 faces (glass does not occlude it)
    CHECK(mesh.opaque.triangleCount() == 12);
    // Water: 2 blocks x 6 faces - 2 (between them) = 10. Glass: 6 - 1 (against stone) = 5
    CHECK(mesh.transparent.triangleCount() == 2 * (10 + 5));
}

TEST_CASE("Mesher: cross-shaped blocks emit 4 quads (two planes, two sides)") {
    TestBlocks b;
    VoxelWorld world(b.registry);
    world.loadChunk({0, 0, 0});
    world.setBlock({2, 0, 2}, b.flower);
    world.setBlock({2, 0, 3}, b.stone); // Flower does not occlude stone faces
    const ChunkMesh mesh = buildChunkMesh(world, {0, 0, 0}, naive());
    CHECK(mesh.transparent.triangleCount() == 8);
    CHECK(mge_test::windingMatchesNormals(mesh.transparent));
    CHECK(mesh.opaque.triangleCount() == 12);
}

TEST_CASE("Mesher: neighbors are read from the world (adjacent chunk loaded vs unloaded)") {
    TestBlocks b;
    VoxelWorld world(b.registry);
    world.loadChunk({0, 0, 0});
    world.setBlock({15, 0, 0}, b.stone);
    CHECK(buildChunkMesh(world, {0, 0, 0}, naive()).opaque.triangleCount() == 12); // Unloaded neighbor = air

    world.loadChunk({1, 0, 0});
    world.setBlock({16, 0, 0}, b.stone); // On the other side of the border
    CHECK(buildChunkMesh(world, {0, 0, 0}, naive()).opaque.triangleCount() == 10);
    CHECK(buildChunkMesh(world, {1, 0, 0}, naive()).opaque.triangleCount() == 10);
}

TEST_CASE("Mesher: per-face atlas tiles; unloaded chunk produces empty meshes") {
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
    // Each vertex falls inside its face's tile according to its normal
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

    CHECK(buildChunkMesh(world, {5, 0, 0}, naive()).opaque.empty());
}

namespace {
    // Total mesh face area per normal direction (for comparing greedy against naive)
    std::array<float, 6> areaByNormal(const MeshData& mesh) {
        std::array<float, 6> area{};
        for (std::size_t t = 0; t + 2 < mesh.indices.size(); t += 3) {
            const glm::vec3 a = mesh.positions[mesh.indices[t]];
            const glm::vec3 b = mesh.positions[mesh.indices[t + 1]];
            const glm::vec3 c = mesh.positions[mesh.indices[t + 2]];
            const glm::vec3 n = mesh.normals[mesh.indices[t]];
            const int axis = std::abs(n.x) > 0.5f ? 0 : std::abs(n.y) > 0.5f ? 1 : 2;
            const int side = (n[axis] > 0.0f) ? 0 : 1;
            area[axis * 2 + side] += 0.5f * glm::length(glm::cross(b - a, c - a));
        }
        return area;
    }

    // Reproducible random block generator: useful for catching border bugs
    class NoiseGenerator : public TerrainGenerator {
    public:
        explicit NoiseGenerator(const TestBlocks& b) : m_b(b) {}
        void generate(const ChunkCoord& coord, ChunkData& chunk) const override {
            const glm::ivec3 o = chunkOrigin(coord);
            for (int y = 0; y < kChunkSize; y++)
                for (int z = 0; z < kChunkSize; z++)
                    for (int x = 0; x < kChunkSize; x++) {
                        const std::uint32_t h = hash3i(o.x + x, o.y + y, o.z + z, 99) % 10;
                        chunk.set(x, y, z, h < 4 ? m_b.stone : h < 5 ? m_b.water : h < 6 ? m_b.glass : kAir);
                    }
        }

    private:
        const TestBlocks& m_b;
    };

    // Independent mesher reference: visible faces counted via world.getBlock across the whole world
    std::size_t expectedFaces(const VoxelWorld& world, const ChunkCoord& coord, bool opaque) {
        const BlockRegistry& r = world.registry();
        const glm::ivec3 dirs[6] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
        std::size_t faces = 0;
        const glm::ivec3 o = chunkOrigin(coord);
        for (int y = 0; y < kChunkSize; y++)
            for (int z = 0; z < kChunkSize; z++)
                for (int x = 0; x < kChunkSize; x++) {
                    const glm::ivec3 p = o + glm::ivec3(x, y, z);
                    const BlockId id = world.getBlock(p);
                    if (id == kAir || r.isOpaque(id) != opaque) continue;
                    for (const glm::ivec3& d : dirs) {
                        const BlockId n = world.getBlock(p + d);
                        if (r.isOpaque(n)) continue;
                        if (!r.isOpaque(id) && n == id) continue;
                        faces++;
                    }
                }
        return faces;
    }
}

TEST_CASE("Mesher: seamless, each chunk matches the reference that inspects the whole world") {
    TestBlocks b;
    VoxelWorld world(b.registry, std::make_unique<NoiseGenerator>(b));
    for (int x = -1; x <= 1; x++)
        for (int y = -1; y <= 1; y++)
            for (int z = -1; z <= 1; z++) world.loadChunk({x, y, z});

    const ChunkCoord center{0, 0, 0}; // All neighbors (including diagonals) are loaded
    const ChunkMesh mesh = buildChunkMesh(world, center, naive());
    CHECK(mesh.opaque.triangleCount() == 2 * expectedFaces(world, center, true));
    CHECK(mesh.transparent.triangleCount() == 2 * expectedFaces(world, center, false));
    // And on a corner chunk (unloaded neighbors = air on both sides)
    const ChunkCoord corner{1, 1, 1};
    CHECK(buildChunkMesh(world, corner, naive()).opaque.triangleCount() ==
          2 * expectedFaces(world, corner, true));
}

TEST_CASE("Mesher: loading a neighbor dirties the chunk and the new mesh culls the border face") {
    TestBlocks b;
    VoxelWorld world(b.registry, std::make_unique<mge_test::FlatGenerator>(b.stone, 4));
    world.loadChunk({0, 0, 0});
    world.clearDirty({0, 0, 0});
    const std::size_t alone = buildChunkMesh(world, {0, 0, 0}, naive()).opaque.triangleCount();

    world.loadChunk({1, 0, 0});
    CHECK(world.isDirty({0, 0, 0}));
    const std::size_t withNeighbour = buildChunkMesh(world, {0, 0, 0}, naive()).opaque.triangleCount();
    CHECK(alone - withNeighbour == 2 * 4 * kChunkSize); // The +X wall (4 high x 16) disappears
}

TEST_CASE("Greedy: fewer triangles than naive while covering the same area") {
    TestBlocks b;
    VoxelWorld world(b.registry, std::make_unique<mge_test::FlatGenerator>(b.stone, 4));
    world.loadChunk({0, 0, 0});
    const MeshData flatNaive = buildChunkMesh(world, {0, 0, 0}, naive()).opaque;
    const MeshData flatGreedy = buildChunkMesh(world, {0, 0, 0}).opaque;
    CHECK(flatGreedy.triangleCount() == 12); // 16x4x16 slab: one quad per side
    CHECK(flatNaive.triangleCount() == 2 * (2 * 16 * 16 + 4 * 4 * 16));
    CHECK(flatGreedy.validate().empty());
    CHECK(mge_test::windingMatchesNormals(flatGreedy));
    const auto areaN = areaByNormal(flatNaive);
    const auto areaG = areaByNormal(flatGreedy);
    for (int i = 0; i < 6; i++) CHECK(areaG[i] == doctest::Approx(areaN[i]));

    // Irregular terrain with multiple block types: same area, fewer triangles
    VoxelWorld rough(b.registry, std::make_unique<NoiseGenerator>(b));
    rough.loadChunk({0, 0, 0});
    const ChunkMesh n = buildChunkMesh(rough, {0, 0, 0}, naive());
    const ChunkMesh g = buildChunkMesh(rough, {0, 0, 0});
    CHECK(g.opaque.triangleCount() < n.opaque.triangleCount());
    CHECK(g.transparent.triangleCount() < n.transparent.triangleCount());
    const auto roughN = areaByNormal(n.opaque);
    const auto roughG = areaByNormal(g.opaque);
    for (int i = 0; i < 6; i++) CHECK(roughG[i] == doctest::Approx(roughN[i]));
}

TEST_CASE("Greedy: textured blocks are not merged (one quad per face)") {
    BlockRegistry registry;
    BlockDesc brick;
    brick.name = "brick";
    brick.setTile(7);
    const BlockId id = registry.add(brick);
    VoxelWorld world(registry, std::make_unique<mge_test::FlatGenerator>(id, 1));
    world.loadChunk({0, 0, 0});
    CHECK(buildChunkMesh(world, {0, 0, 0}).opaque.triangleCount() ==
          buildChunkMesh(world, {0, 0, 0}, naive()).opaque.triangleCount());
}

TEST_CASE("AO: darkens floor corners next to a wall; disabled leaves color unchanged") {
    TestBlocks b;
    VoxelWorld world(b.registry);
    world.loadChunk({0, 0, 0});
    for (int x = 0; x < 3; x++)
        for (int z = 0; z < 3; z++) world.setBlock({x, 0, z}, b.stone); // 3x3 floor
    world.setBlock({0, 1, 1}, b.stone);                                 // Block on top of the floor

    const glm::vec4 base = b.registry.get(b.stone).color;
    for (const glm::vec4& c : buildChunkMesh(world, {0, 0, 0}, naive()).opaque.colors) CHECK(c == base);

    ChunkMeshOptions options = naive();
    options.ambientOcclusion = true;
    const MeshData mesh = buildChunkMesh(world, {0, 0, 0}, options).opaque;
    // Floor top vertex at (1, 1, 1): touches the wall -> darker. At (3, 1, 3): unobstructed
    bool darkFound = false, brightFound = false;
    for (std::size_t i = 0; i < mesh.vertexCount(); i++) {
        if (mesh.normals[i].y < 0.5f) continue;
        if (mesh.positions[i] == glm::vec3(1, 1, 1)) darkFound |= mesh.colors[i].r < base.r - 1e-4f;
        if (mesh.positions[i] == glm::vec3(3, 1, 3))
            brightFound |= mesh.colors[i].r == doctest::Approx(base.r);
    }
    CHECK(darkFound);
    CHECK(brightFound);
    CHECK(mge_test::windingMatchesNormals(mesh));

    // With greedy + AO: still covers the same area
    options.greedy = true;
    const auto areaG = areaByNormal(buildChunkMesh(world, {0, 0, 0}, options).opaque);
    const auto areaN = areaByNormal(mesh);
    for (int i = 0; i < 6; i++) CHECK(areaG[i] == doctest::Approx(areaN[i]));
}

TEST_CASE("Mesher: textured blocks use white unless tintTextured is enabled") {
    BlockRegistry registry;
    BlockDesc brick;
    brick.name = "brick";
    brick.color = {0.8f, 0.2f, 0.1f, 1.0f};
    brick.setTile(7);
    const BlockId textured = registry.add(brick);
    BlockDesc paint;
    paint.name = "paint";
    paint.color = {0.1f, 0.6f, 0.3f, 1.0f};
    const BlockId colorOnly = registry.add(paint);

    VoxelWorld world(registry);
    world.loadChunk({0, 0, 0});
    world.setBlock({0, 0, 0}, textured);
    world.setBlock({5, 0, 0}, colorOnly);

    ChunkMeshOptions options = naive();
    const MeshData white = buildChunkMesh(world, {0, 0, 0}, options).opaque;
    options.tintTextured = true;
    const MeshData tinted = buildChunkMesh(world, {0, 0, 0}, options).opaque;
    REQUIRE(white.vertexCount() == tinted.vertexCount());
    for (std::size_t i = 0; i < white.vertexCount(); i++) {
        const bool isBrick = white.positions[i].x <= 1.0f;
        CAPTURE(i);
        CHECK(white.colors[i] == (isBrick ? glm::vec4(1.0f) : paint.color)); // Untextured: always its color
        CHECK(tinted.colors[i] == (isBrick ? brick.color : paint.color));
    }
}

TEST_CASE("Mesher: each block goes to its RenderLayer mesh") {
    BlockRegistry registry;
    BlockDesc stone;
    stone.name = "stone";
    BlockDesc leaves;
    leaves.name = "leaves";
    leaves.opaque = false;
    leaves.layer = RenderLayer::Cutout;
    BlockDesc water;
    water.name = "water";
    water.opaque = false; // No explicit layer: translucent by default
    CHECK(stone.renderLayer() == RenderLayer::Opaque);
    CHECK(leaves.renderLayer() == RenderLayer::Cutout);
    CHECK(water.renderLayer() == RenderLayer::Translucent);

    const BlockId s = registry.add(stone), l = registry.add(leaves), w = registry.add(water);
    VoxelWorld world(registry);
    world.loadChunk({0, 0, 0});
    world.setBlock({0, 0, 0}, s);
    world.setBlock({4, 0, 0}, l);
    world.setBlock({8, 0, 0}, w);
    const ChunkMesh mesh = buildChunkMesh(world, {0, 0, 0}, naive());
    CHECK(mesh.opaque.triangleCount() == 12);
    CHECK(mesh.cutout.triangleCount() == 12);
    CHECK(mesh.transparent.triangleCount() == 12);
    CHECK(mesh.transparentFaceCentroids.size() * 6 == mesh.transparent.indices.size());
    CHECK(mesh.cutout.bounds().min.x == doctest::Approx(4.0f));
}

TEST_CASE("Mesher: sortQuadsBackToFront sorts 4 parallel quads from both sides (#180)") {
    // 4 parallel quads at z = 1, 3, 5, 7 with distinct indices per quad
    std::vector<std::uint32_t> indices;
    std::vector<glm::vec3> centroids;
    const float zs[4] = {1.0f, 3.0f, 5.0f, 7.0f};
    for (std::uint32_t q = 0; q < 4; ++q) {
        const std::uint32_t base = q * 4;
        indices.insert(indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
        centroids.emplace_back(4.0f, 4.0f, zs[q]);
    }

    // From z = -5 (front): farthest is q=3 (z=7), nearest is q=0 (z=1)
    const std::vector<std::uint32_t> fromFront =
        sortQuadsBackToFront(indices, centroids, {4.0f, 4.0f, -5.0f});
    REQUIRE(fromFront.size() == indices.size());
    CHECK(fromFront[0] == 12); // q=3
    CHECK(fromFront[6] == 8);  // q=2
    CHECK(fromFront[12] == 4); // q=1
    CHECK(fromFront[18] == 0); // q=0

    // From z = 15 (back): farthest is q=0 (z=1), nearest is q=3 (z=7)
    const std::vector<std::uint32_t> fromBack = sortQuadsBackToFront(indices, centroids, {4.0f, 4.0f, 15.0f});
    REQUIRE(fromBack.size() == indices.size());
    CHECK(fromBack[0] == 0);   // q=0
    CHECK(fromBack[6] == 4);   // q=1
    CHECK(fromBack[12] == 8);  // q=2
    CHECK(fromBack[18] == 12); // q=3
}
