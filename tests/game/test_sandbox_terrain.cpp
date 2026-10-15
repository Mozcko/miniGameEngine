#include <doctest/doctest.h>
#include <game/SandboxBlocks.h>
#include <game/SandboxTerrain.h>

#include <initializer_list>
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

TEST_CASE("Terreno: un solo chunk de alto (los de arriba y abajo quedan vacíos)") {
    Fixture f;
    CHECK(f.generate(0, 0, SandboxTerrain::kDefaultSeed, 1).isEmpty());
    CHECK(f.generate(0, 0, SandboxTerrain::kDefaultSeed, -1).isEmpty());
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

TEST_CASE("Terreno: terrainHeight dentro de [1, kWorldHeight - 1]") {
    for (int x = -200; x < 200; x += 7)
        for (int z = -200; z < 200; z += 5) {
            const int h = SandboxTerrain::terrainHeight(x, z);
            CHECK(h >= 1);
            CHECK(h < SandboxTerrain::kWorldHeight);
        }
}
