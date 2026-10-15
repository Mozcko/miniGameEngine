#include <doctest/doctest.h>
#include <Game/Chunk.h>
#include <initializer_list>
#include <memory>
#include <utility>

using namespace MiniGameEngine;

// Estos tests solo usan la generación (CPU). No llaman a buildMesh(), así que
// no necesitan ventana ni contexto OpenGL.

namespace {
    const int WATER_LEVEL = 5; // Igual que en Chunk::generate

    bool sameBlocks(const Chunk& a, const Chunk& b) {
        for (int x = 0; x < CHUNK_SIZE; x++)
            for (int y = 0; y < CHUNK_HEIGHT; y++)
                for (int z = 0; z < CHUNK_SIZE; z++)
                    if (a.getBlock(x, y, z).getType() != b.getBlock(x, y, z).getType())
                        return false;
        return true;
    }
}

TEST_CASE("La generación es determinista") {
    for (const auto& coords : {std::pair{0, 0}, std::pair{3, -2}, std::pair{-7, 11}}) {
        const int cx = coords.first;
        const int cz = coords.second;
        CAPTURE(cx);
        CAPTURE(cz);
        auto a = std::make_unique<Chunk>(cx, cz);
        auto b = std::make_unique<Chunk>(cx, cz);
        CHECK(sameBlocks(*a, *b));
    }
}

TEST_CASE("Chunks distintos generan terreno distinto") {
    auto a = std::make_unique<Chunk>(0, 0);
    auto b = std::make_unique<Chunk>(1, 0);
    CHECK_FALSE(sameBlocks(*a, *b));
}

TEST_CASE("getBlock fuera de rango devuelve Air") {
    auto c = std::make_unique<Chunk>(0, 0);
    CHECK(c->getBlock(-1, 0, 0).getType() == BlockType::Air);
    CHECK(c->getBlock(CHUNK_SIZE, 0, 0).getType() == BlockType::Air);
    CHECK(c->getBlock(0, -1, 0).getType() == BlockType::Air);
    CHECK(c->getBlock(0, CHUNK_HEIGHT, 0).getType() == BlockType::Air);
    CHECK(c->getBlock(0, 0, CHUNK_SIZE).getType() == BlockType::Air);
}

TEST_CASE("Reglas del terreno en un área de 8x8 chunks") {
    for (int cx = -4; cx < 4; cx++) {
        for (int cz = -4; cz < 4; cz++) {
            auto c = std::make_unique<Chunk>(cx, cz);
            for (int x = 0; x < CHUNK_SIZE; x++) {
                for (int z = 0; z < CHUNK_SIZE; z++) {
                    // La altura mínima es 1: el fondo siempre es sólido
                    CHECK(c->getBlock(x, 0, z).isSolid());

                    for (int y = 0; y < CHUNK_HEIGHT; y++) {
                        BlockType t = c->getBlock(x, y, z).getType();
                        // El agua nunca supera el nivel del mar
                        if (t == BlockType::Water) {
                            CHECK(y <= WATER_LEVEL);
                        }
                        // Bajo el agua no hay aire (el agua llena hasta el suelo)
                        if (t == BlockType::Water && y > 0) {
                            CHECK(c->getBlock(x, y - 1, z).isActive());
                        }
                    }
                }
            }
        }
    }
}

TEST_CASE("Los chunks recién creados no tienen malla hasta buildMesh") {
    // Crear y destruir sin buildMesh no debe tocar la GPU ni fallar
    auto c = std::make_unique<Chunk>(2, 2);
    CHECK(c->getPosition().x == doctest::Approx(2.0f * CHUNK_SIZE));
    CHECK(c->getPosition().z == doctest::Approx(2.0f * CHUNK_SIZE));
}

TEST_CASE("La decoración genera árboles, rosas y flores (regresión #5)") {
    // Antes del fix, la pasada de decoración repetía la tirada de la grava:
    // nunca salían árboles ni rosas, solo flores (~1%).
    long logs = 0, leaves = 0, roses = 0, flowers = 0, trees = 0, grassColumns = 0;

    for (int cx = -10; cx < 10; cx++) {
        for (int cz = -10; cz < 10; cz++) {
            auto c = std::make_unique<Chunk>(cx, cz);
            for (int x = 0; x < CHUNK_SIZE; x++) {
                for (int z = 0; z < CHUNK_SIZE; z++) {
                    for (int y = 0; y < CHUNK_HEIGHT; y++) {
                        switch (c->getBlock(x, y, z).getType()) {
                            case BlockType::Log:    logs++; break;
                            case BlockType::Leaves: leaves++; break;
                            case BlockType::Rose:   roses++; break;
                            case BlockType::Flower: flowers++; break;
                            default: break;
                        }
                    }

                    // Superficie: primer bloque desde arriba que no sea aire ni decoración
                    for (int y = CHUNK_HEIGHT - 1; y >= 0; y--) {
                        BlockType t = c->getBlock(x, y, z).getType();
                        if (t == BlockType::Air || t == BlockType::Leaves || t == BlockType::Log ||
                            t == BlockType::Rose || t == BlockType::Flower)
                            continue;
                        if (t == BlockType::Grass) {
                            grassColumns++;
                            // Un tronco apoyado sobre el pasto = un árbol
                            if (c->getBlock(x, y + 1, z).getType() == BlockType::Log) trees++;
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
    CHECK(roses > 0);
    CHECK(flowers > 0);

    // Cada decoración tiene un 2% de probabilidad sobre el pasto
    REQUIRE(grassColumns > 1000);
    const double treeRate = double(trees) / double(grassColumns);
    const double roseRate = double(roses) / double(grassColumns);
    const double flowerRate = double(flowers) / double(grassColumns);
    CHECK(treeRate > 0.01);
    CHECK(treeRate < 0.03);
    CHECK(roseRate > 0.01);
    CHECK(roseRate < 0.03);
    CHECK(flowerRate > 0.01);
    CHECK(flowerRate < 0.03);
}
