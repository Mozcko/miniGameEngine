#include <doctest/doctest.h>
#include <game/SandboxBlocks.h>
#include <game/SandboxTerrain.h>

#include <array>
#include <cstdlib>
#include <initializer_list>
#include <memory>
#include <utility>

using namespace mge;
using sandbox::Biome;
using sandbox::SandboxTerrain;
using sandbox::TerrainSettings;

// Solo generación (CPU): sin ventana ni contexto OpenGL.

namespace {
    TerrainSettings withSeed(std::uint32_t seed, bool caves = true) {
        TerrainSettings settings;
        settings.seed = seed;
        settings.caves = caves;
        return settings;
    }

    struct Fixture {
        BlockRegistry registry;
        sandbox::SandboxBlocks b = sandbox::registerSandboxBlocks(registry);

        ChunkData generate(const ChunkCoord& coord, TerrainSettings settings = {}) const {
            ChunkData chunk;
            SandboxTerrain(b, settings).generate(coord, chunk);
            return chunk;
        }

        // Mundo de columnas cx, cz en [-r, r] con todos sus chunks verticales, cargado en el orden indicado
        std::unique_ptr<VoxelWorld> world(int r, bool reverse = false, TerrainSettings settings = {}) const {
            auto w = std::make_unique<VoxelWorld>(registry, std::make_unique<SandboxTerrain>(b, settings));
            for (int i = -r; i <= r; i++)
                for (int k = -r; k <= r; k++)
                    for (int cy = 0; cy < SandboxTerrain::kWorldChunksY; cy++)
                        w->loadChunk(reverse ? ChunkCoord{-i, SandboxTerrain::kWorldChunksY - 1 - cy, -k}
                                             : ChunkCoord{i, cy, k});
            return w;
        }
    };
}

TEST_CASE("Terreno: determinista, distinto entre chunks y según la semilla") {
    Fixture f;
    for (const ChunkCoord& c : {ChunkCoord{0, 1, 0}, ChunkCoord{3, 0, -2}, ChunkCoord{-7, 2, 11}}) {
        CAPTURE(c.x);
        CAPTURE(c.y);
        CAPTURE(c.z);
        CHECK(f.generate(c).blocks() == f.generate(c).blocks());
    }
    CHECK(f.generate({0, 1, 0}).blocks() != f.generate({1, 1, 0}).blocks());
    CHECK(f.generate({0, 1, 0}, withSeed(1)).blocks() != f.generate({0, 1, 0}, withSeed(2)).blocks());
    // La altura ahora sí depende de la semilla (ruido)
    const SandboxTerrain a(f.b, withSeed(1)), b(f.b, withSeed(2));
    int differ = 0;
    for (int x = 0; x < 64; x += 4) differ += a.surfaceHeight(x, 0) != b.surfaceHeight(x, 0);
    CHECK(differ > 8);
}

TEST_CASE("Terreno: debajo del mundo es roca; arriba del mundo no hay nada") {
    Fixture f;
    const ChunkData below = f.generate({0, -1, 0});
    for (BlockId id : below.blocks()) CHECK(id == f.b.stone);
    for (int cx = -3; cx < 3; cx++) {
        const ChunkData above = f.generate({cx, SandboxTerrain::kWorldChunksY, 0});
        CHECK(above.isEmpty());
    }
}

TEST_CASE("Terreno: alturas en rango y sin repetirse como las olas de antes") {
    Fixture f;
    const SandboxTerrain terrain(f.b);
    int minH = 1000, maxH = -1;
    for (int x = -2000; x < 2000; x += 13)
        for (int z = -2000; z < 2000; z += 17) {
            const int h = terrain.surfaceHeight(x, z);
            CHECK(h >= 1);
            CHECK(h <= SandboxTerrain::kMaxSurfaceHeight);
            minH = std::min(minH, h);
            maxH = std::max(maxH, h);
        }
    CAPTURE(minH);
    CAPTURE(maxH);
    CHECK(minH < SandboxTerrain::kWaterLevel);      // Hay mar
    CHECK(maxH > SandboxTerrain::kWaterLevel + 20); // Hay montañas
}

TEST_CASE("Terreno: aparecen todos los biomas en un área grande") {
    Fixture f;
    const SandboxTerrain terrain(f.b);
    std::array<int, 4> counts{};
    int total = 0;
    for (int x = -3000; x < 3000; x += 24)
        for (int z = -3000; z < 3000; z += 24) {
            counts[static_cast<int>(terrain.biome(x, z))]++;
            total++;
        }
    const char* names[] = {"Plains", "Desert", "Mountains", "Shore"};
    for (int i = 0; i < 4; i++) {
        CAPTURE(names[i]);
        CAPTURE(counts[i]);
        CHECK(counts[i] > total / 50); // Cada uno > 2 %
    }
    CHECK(counts[static_cast<int>(Biome::Plains)] > total / 5); // Las llanuras dominan
}

TEST_CASE("Terreno: reglas de bloques en un mundo de 5×5 columnas") {
    Fixture f;
    const SandboxTerrain terrain(f.b);
    const auto world = f.world(2);
    for (int x = -32; x < 48; x++) {
        for (int z = -32; z < 48; z++) {
            CHECK(world->getBlock(x, 0, z) == f.b.stone); // Las cuevas nunca rompen el piso
            const int h = terrain.surfaceHeight(x, z);
            for (int y = 1; y < SandboxTerrain::kWorldHeight; y++) {
                const BlockId id = world->getBlock(x, y, z);
                if (id == f.b.water) {
                    CHECK(y <= SandboxTerrain::kWaterLevel); // El agua nunca supera el nivel del mar
                    CHECK(y > h);                            // y solo está sobre la superficie
                }
                if (y == h) CHECK(id != kAir); // La superficie nunca se hunde en una cueva
            }
        }
    }
}

TEST_CASE("Terreno: hay cuevas bajo tierra, y apagadas no hay") {
    Fixture f;
    const SandboxTerrain terrain(f.b);
    long underground = 0, carved = 0;
    for (int x = 0; x < 96; x++)
        for (int z = 0; z < 96; z++) {
            const int h = terrain.surfaceHeight(x, z);
            for (int y = 1; y <= h - 4; y++) {
                underground++;
                carved += terrain.isCave(x, y, z, h);
            }
        }
    CAPTURE(carved);
    CAPTURE(underground);
    CHECK(carved > underground / 200); // > 0.5 %
    CHECK(carved < underground / 5);   // Pero el subsuelo sigue siendo mayormente roca

    const SandboxTerrain noCaves(f.b, withSeed(SandboxTerrain::kDefaultSeed, false));
    for (int x = 0; x < 32; x++)
        for (int y = 1; y < 30; y++) CHECK_FALSE(noCaves.isCave(x, y, 0, 50));
}

TEST_CASE("Terreno: la decoración genera árboles, rosas y flores en 1–3 % del pasto (regresión #5)") {
    Fixture f;
    const SandboxTerrain terrain(f.b);
    const auto world = f.world(5);
    long logs = 0, leaves = 0, roses = 0, flowers = 0, trees = 0, grassColumns = 0;
    for (int x = -80; x < 96; x++) {
        for (int z = -80; z < 96; z++) {
            const int h = terrain.surfaceHeight(x, z);
            if (world->getBlock(x, h, z) != f.b.grass) continue;
            grassColumns++;
            const BlockId above = world->getBlock(x, h + 1, z);
            trees += above == f.b.log; // Tronco sobre pasto = árbol
            roses += above == f.b.rose;
            flowers += above == f.b.flower;
            for (int y = h + 1; y < h + 9; y++) {
                logs += world->getBlock(x, y, z) == f.b.log;
                leaves += world->getBlock(x, y, z) == f.b.leaves;
            }
        }
    }
    CAPTURE(grassColumns);
    CAPTURE(trees);
    CAPTURE(roses);
    CAPTURE(flowers);
    CHECK(logs > 0);
    CHECK(leaves > 0);
    REQUIRE(grassColumns > 5000);
    for (long count : {trees, roses, flowers}) {
        const double rate = double(count) / double(grassColumns);
        CHECK(rate > 0.01);
        CHECK(rate < 0.03);
    }
}

TEST_CASE("Terreno: coordenadas lejanas no rompen la generación (sin overflow)") {
    Fixture f;
    const ChunkData far = f.generate({100000, 0, -100000});
    CHECK(f.registry.isSolid(far.get(0, 0, 0)));
}

TEST_CASE("Terreno: los árboles cruzan bordes de chunk enteros (KI#6)") {
    Fixture f;
    const SandboxTerrain terrain(f.b);
    const auto world = f.world(3);
    int trees = 0, borderTrees = 0;
    // Para cada tronco sobre pasto lejos del borde del área cargada: su copa entera está en el mundo
    for (int x = -46; x < 62; x++) {
        for (int z = -46; z < 62; z++) {
            const int h = terrain.surfaceHeight(x, z);
            if (world->getBlock(x, h, z) != f.b.grass || world->getBlock(x, h + 1, z) != f.b.log) continue;
            trees++;
            int trunk = 0;
            while (world->getBlock(x, h + 1 + trunk, z) == f.b.log) trunk++;
            const int top = h + trunk; // Último bloque del tronco
            const int lx = floorMod(x, kChunkSize), lz = floorMod(z, kChunkSize),
                      ly = floorMod(top, kChunkSize);
            if (lx < 2 || lx > 13 || lz < 2 || lz > 13 || ly > 13) borderTrees++;
            // Capa de arriba de la copa (por encima del tronco): rombo completo de hojas
            for (int dx = -2; dx <= 2; dx++)
                for (int dz = -2; dz <= 2; dz++) {
                    if (std::abs(dx) + std::abs(dz) >= 4) continue;
                    const BlockId id = world->getBlock(x + dx, top + 2, z + dz);
                    CAPTURE(x);
                    CAPTURE(z);
                    CHECK((id == f.b.leaves || id == f.b.log));
                }
        }
    }
    CAPTURE(trees);
    CHECK(trees > 20);
    CHECK(borderTrees > 5); // Hay árboles que tocan otro chunk (al costado o arriba)
}

TEST_CASE("Terreno: el mundo es igual sin importar el orden de carga") {
    Fixture f;
    const auto a = f.world(2, false);
    const auto b = f.world(2, true);
    for (const ChunkCoord& c : a->loadedChunks()) CHECK(a->chunk(c)->blocks() == b->chunk(c)->blocks());
}
