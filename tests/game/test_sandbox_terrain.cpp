#include <doctest/doctest.h>
#include <game/SandboxBlocks.h>
#include <game/SandboxTerrain.h>

#include <cstdlib>
#include <initializer_list>
#include <memory>
#include <utility>

using namespace mge;
using sandbox::SandboxTerrain;

// Solo generación (CPU): sin ventana ni contexto OpenGL.

namespace {
    struct Fixture {
        BlockRegistry registry;
        sandbox::SandboxBlocks b = sandbox::registerSandboxBlocks(registry);

        ChunkData generate(int cx, int cz, std::uint32_t seed = SandboxTerrain::kDefaultSeed,
                           int cy = 0) const {
            ChunkData chunk;
            SandboxTerrain(b, seed).generate({cx, cy, cz}, chunk);
            return chunk;
        }
    };
}

TEST_CASE("Terreno: determinista y distinto entre chunks") {
    Fixture f;
    for (const auto& c : {std::pair{0, 0}, std::pair{3, -2}, std::pair{-7, 11}}) {
        CAPTURE(c.first);
        CAPTURE(c.second);
        CHECK(f.generate(c.first, c.second).blocks() == f.generate(c.first, c.second).blocks());
    }
    CHECK(f.generate(0, 0).blocks() != f.generate(1, 0).blocks());
}

TEST_CASE("Terreno: debajo del mundo es roca; arriba solo copas de árboles") {
    Fixture f;
    const ChunkData below = f.generate(0, 0, SandboxTerrain::kDefaultSeed, -1);
    for (BlockId id : below.blocks()) CHECK(id == f.b.stone);
    for (int cx = -3; cx < 3; cx++) {
        const ChunkData above = f.generate(cx, 0, SandboxTerrain::kDefaultSeed, 1);
        for (BlockId id : above.blocks()) CHECK((id == kAir || id == f.b.leaves || id == f.b.log));
    }
}

TEST_CASE("Terreno: reglas en un área de 8×8 chunks") {
    Fixture f;
    for (int cx = -4; cx < 4; cx++) {
        for (int cz = -4; cz < 4; cz++) {
            const ChunkData c = f.generate(cx, cz);
            for (int x = 0; x < kChunkSize; x++) {
                for (int z = 0; z < kChunkSize; z++) {
                    // La altura mínima es 1: el fondo siempre es sólido
                    CHECK(f.registry.isSolid(c.get(x, 0, z)));
                    for (int y = 0; y < kChunkSize; y++) {
                        if (c.get(x, y, z) != f.b.water) continue;
                        CHECK(y <= SandboxTerrain::kWaterLevel);      // El agua nunca supera el nivel del mar
                        if (y > 0) CHECK(c.get(x, y - 1, z) != kAir); // Ni flota sobre aire
                    }
                }
            }
        }
    }
}

TEST_CASE("Terreno: la decoración genera árboles, rosas y flores en 1–3 % del pasto (regresión #5)") {
    Fixture f;
    long logs = 0, leaves = 0, roses = 0, flowers = 0, trees = 0, grassColumns = 0;

    for (int cx = -10; cx < 10; cx++) {
        for (int cz = -10; cz < 10; cz++) {
            const ChunkData c = f.generate(cx, cz);
            for (int x = 0; x < kChunkSize; x++) {
                for (int z = 0; z < kChunkSize; z++) {
                    for (int y = 0; y < kChunkSize; y++) {
                        const BlockId id = c.get(x, y, z);
                        logs += id == f.b.log;
                        leaves += id == f.b.leaves;
                        roses += id == f.b.rose;
                        flowers += id == f.b.flower;
                    }
                    // Superficie: primer bloque desde arriba que no sea aire ni decoración
                    for (int y = kChunkSize - 1; y >= 0; y--) {
                        const BlockId id = c.get(x, y, z);
                        if (id == kAir || id == f.b.leaves || id == f.b.log || id == f.b.rose ||
                            id == f.b.flower)
                            continue;
                        if (id == f.b.grass) {
                            grassColumns++;
                            if (c.get(x, y + 1, z) == f.b.log) trees++; // Tronco sobre pasto = árbol
                        }
                        break;
                    }
                }
            }
        }
    }

    CAPTURE(grassColumns);
    CAPTURE(trees);
    CAPTURE(roses);
    CAPTURE(flowers);
    CHECK(logs > 0);
    CHECK(leaves > 0);
    REQUIRE(grassColumns > 1000);
    for (long count : {trees, roses, flowers}) {
        const double rate = double(count) / double(grassColumns);
        CHECK(rate > 0.01);
        CHECK(rate < 0.03);
    }
}

TEST_CASE("Terreno: la semilla cambia la decoración; la misma semilla la repite") {
    Fixture f;
    CHECK(f.generate(0, 0, 1u).blocks() == f.generate(0, 0, 1u).blocks());
    // La altura todavía no depende de la semilla (sin/cos, #27), pero la grava y la decoración sí
    CHECK(f.generate(0, 0, 1u).blocks() != f.generate(0, 0, 2u).blocks());
}

TEST_CASE("Terreno: coordenadas lejanas no rompen la generación (sin overflow)") {
    Fixture f;
    CHECK(f.registry.isSolid(f.generate(100000, -100000).get(0, 0, 0)));
}

TEST_CASE("Terreno: terrainHeight dentro de [1, kMaxSurfaceHeight]") {
    for (int x = -200; x < 200; x += 7)
        for (int z = -200; z < 200; z += 5) {
            const int h = SandboxTerrain::terrainHeight(x, z);
            CHECK(h >= 1);
            CHECK(h <= SandboxTerrain::kMaxSurfaceHeight);
        }
}

namespace {
    // Mundo de chunks cx, cz en [-r, r] (y = 0 y 1), cargados en el orden indicado
    std::unique_ptr<VoxelWorld> loadWorld(const Fixture& f, int r, bool reverse) {
        auto world = std::make_unique<VoxelWorld>(f.registry, std::make_unique<SandboxTerrain>(f.b));
        for (int i = -r; i <= r; i++)
            for (int k = -r; k <= r; k++)
                for (int cy = 0; cy <= 1; cy++)
                    world->loadChunk(reverse ? ChunkCoord{-i, cy, -k} : ChunkCoord{i, cy, k});
        return world;
    }
}

TEST_CASE("Terreno: los árboles cruzan bordes de chunk enteros (KI#6)") {
    Fixture f;
    const auto world = loadWorld(f, 3, false);
    int trees = 0, borderTrees = 0;
    // Para cada tronco sobre pasto lejos del borde del área cargada: su copa entera está en el mundo
    for (int x = -46; x < 46; x++) {
        for (int z = -46; z < 46; z++) {
            const int h = SandboxTerrain::terrainHeight(x, z);
            if (world->getBlock(x, h, z) != f.b.grass || world->getBlock(x, h + 1, z) != f.b.log) continue;
            trees++;
            int trunk = 0;
            while (world->getBlock(x, h + 1 + trunk, z) == f.b.log) trunk++;
            const int top = h + trunk; // Último bloque del tronco
            const int lx = floorMod(x, kChunkSize), lz = floorMod(z, kChunkSize);
            if (lx < 2 || lx > 13 || lz < 2 || lz > 13) borderTrees++;
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
    CHECK(borderTrees > 5); // Hay árboles que tocan otro chunk
}

TEST_CASE("Terreno: el mundo es igual sin importar el orden de carga") {
    Fixture f;
    const auto a = loadWorld(f, 2, false);
    const auto b = loadWorld(f, 2, true);
    for (const ChunkCoord& c : a->loadedChunks()) CHECK(a->chunk(c)->blocks() == b->chunk(c)->blocks());
}
