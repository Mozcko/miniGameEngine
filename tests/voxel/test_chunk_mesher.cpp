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
    // Una cara por bloque (sin unir): los conteos son los del mallado "ingenuo"
    ChunkMeshOptions naive() {
        ChunkMeshOptions options;
        options.greedy = false;
        return options;
    }
}

TEST_CASE("Mesher: un bloque suelto es un cubo cerrado de 6 caras") {
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
    const ChunkMesh two = buildChunkMesh(world, {0, 0, 0}, naive());
    CHECK(two.opaque.triangleCount() == 2 * (12 - 2)); // 10 caras visibles
    CHECK(mge_test::isWatertight(two.opaque));

    // Bloque 3×3×3 macizo: solo la superficie (6 lados × 9 caras)
    for (int x = 4; x < 7; x++)
        for (int y = 4; y < 7; y++)
            for (int z = 4; z < 7; z++) world.setBlock({x, y, z}, b.stone);
    const ChunkMesh cube = buildChunkMesh(world, {0, 0, 0}, naive());
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

    const ChunkMesh mesh = buildChunkMesh(world, {0, 0, 0}, naive());
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
    const ChunkMesh mesh = buildChunkMesh(world, {0, 0, 0}, naive());
    CHECK(mesh.transparent.triangleCount() == 8);
    CHECK(mge_test::windingMatchesNormals(mesh.transparent));
    CHECK(mesh.opaque.triangleCount() == 12);
}

TEST_CASE("Mesher: los vecinos se leen del mundo (chunk de al lado cargado / sin cargar)") {
    TestBlocks b;
    VoxelWorld world(b.registry);
    world.loadChunk({0, 0, 0});
    world.setBlock({15, 0, 0}, b.stone);
    CHECK(buildChunkMesh(world, {0, 0, 0}, naive()).opaque.triangleCount() == 12); // Vecino sin cargar = aire

    world.loadChunk({1, 0, 0});
    world.setBlock({16, 0, 0}, b.stone); // Al otro lado del borde
    CHECK(buildChunkMesh(world, {0, 0, 0}, naive()).opaque.triangleCount() == 10);
    CHECK(buildChunkMesh(world, {1, 0, 0}, naive()).opaque.triangleCount() == 10);
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

    CHECK(buildChunkMesh(world, {5, 0, 0}, naive()).opaque.empty());
}

namespace {
    // Área total de las caras de la malla por dirección de normal (para comparar greedy con ingenuo)
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

    // Generador con bloques al azar (reproducible): sirve para buscar errores en los bordes
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

    // Referencia independiente del mesher: caras visibles contando con world.getBlock en todo el mundo
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

TEST_CASE("Mesher: sin costuras, cada chunk coincide con la referencia que mira todo el mundo") {
    TestBlocks b;
    VoxelWorld world(b.registry, std::make_unique<NoiseGenerator>(b));
    for (int x = -1; x <= 1; x++)
        for (int y = -1; y <= 1; y++)
            for (int z = -1; z <= 1; z++) world.loadChunk({x, y, z});

    const ChunkCoord center{0, 0, 0}; // Todos sus vecinos (también en diagonal) están cargados
    const ChunkMesh mesh = buildChunkMesh(world, center, naive());
    CHECK(mesh.opaque.triangleCount() == 2 * expectedFaces(world, center, true));
    CHECK(mesh.transparent.triangleCount() == 2 * expectedFaces(world, center, false));
    // Y en un chunk de la esquina (vecinos sin cargar = aire en ambos lados)
    const ChunkCoord corner{1, 1, 1};
    CHECK(buildChunkMesh(world, corner, naive()).opaque.triangleCount() ==
          2 * expectedFaces(world, corner, true));
}

TEST_CASE("Mesher: cargar un vecino marca sucio al chunk y la nueva malla oculta la cara del borde") {
    TestBlocks b;
    VoxelWorld world(b.registry, std::make_unique<mge_test::FlatGenerator>(b.stone, 4));
    world.loadChunk({0, 0, 0});
    world.clearDirty({0, 0, 0});
    const std::size_t alone = buildChunkMesh(world, {0, 0, 0}, naive()).opaque.triangleCount();

    world.loadChunk({1, 0, 0});
    CHECK(world.isDirty({0, 0, 0}));
    const std::size_t withNeighbour = buildChunkMesh(world, {0, 0, 0}, naive()).opaque.triangleCount();
    CHECK(alone - withNeighbour == 2 * 4 * kChunkSize); // Desaparece la pared +X (4 de alto × 16)
}

TEST_CASE("Greedy: menos triángulos que el ingenuo cubriendo la misma área") {
    TestBlocks b;
    VoxelWorld world(b.registry, std::make_unique<mge_test::FlatGenerator>(b.stone, 4));
    world.loadChunk({0, 0, 0});
    const MeshData flatNaive = buildChunkMesh(world, {0, 0, 0}, naive()).opaque;
    const MeshData flatGreedy = buildChunkMesh(world, {0, 0, 0}).opaque;
    CHECK(flatGreedy.triangleCount() == 12); // Losa de 16×4×16: una cara por lado
    CHECK(flatNaive.triangleCount() == 2 * (2 * 16 * 16 + 4 * 4 * 16));
    CHECK(flatGreedy.validate().empty());
    CHECK(mge_test::windingMatchesNormals(flatGreedy));
    const auto areaN = areaByNormal(flatNaive);
    const auto areaG = areaByNormal(flatGreedy);
    for (int i = 0; i < 6; i++) CHECK(areaG[i] == doctest::Approx(areaN[i]));

    // Terreno irregular con varios bloques: misma área, menos triángulos
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

TEST_CASE("Greedy: los bloques con textura no se unen (un quad por cara)") {
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

TEST_CASE("AO: oscurece las esquinas del piso junto a una pared; apagado no cambia el color") {
    TestBlocks b;
    VoxelWorld world(b.registry);
    world.loadChunk({0, 0, 0});
    for (int x = 0; x < 3; x++)
        for (int z = 0; z < 3; z++) world.setBlock({x, 0, z}, b.stone); // Piso 3×3
    world.setBlock({0, 1, 1}, b.stone);                                 // Bloque sobre el piso

    const glm::vec4 base = b.registry.get(b.stone).color;
    for (const glm::vec4& c : buildChunkMesh(world, {0, 0, 0}, naive()).opaque.colors) CHECK(c == base);

    ChunkMeshOptions options = naive();
    options.ambientOcclusion = true;
    const MeshData mesh = buildChunkMesh(world, {0, 0, 0}, options).opaque;
    // Vértice del techo del piso en (1, 1, 1): toca la pared → más oscuro. En (3, 1, 3): libre
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

    // Con greedy + AO: sigue cubriendo lo mismo
    options.greedy = true;
    const auto areaG = areaByNormal(buildChunkMesh(world, {0, 0, 0}, options).opaque);
    const auto areaN = areaByNormal(mesh);
    for (int i = 0; i < 6; i++) CHECK(areaG[i] == doctest::Approx(areaN[i]));
}
