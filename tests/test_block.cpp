#include <doctest/doctest.h>
#include <Game/Block.h>
#include <initializer_list>

using namespace MiniGameEngine;

TEST_CASE("Air es el único bloque inactivo") {
    CHECK_FALSE(Block().isActive());
    CHECK_FALSE(Block(BlockType::Air).isActive());
    CHECK(Block(BlockType::Stone).isActive());
    CHECK(Block(BlockType::Water).isActive());
}

TEST_CASE("Bloques atravesables: no sólidos y no opacos") {
    for (BlockType t : {BlockType::Air, BlockType::Rose, BlockType::Flower, BlockType::Sapling,
                        BlockType::Cobweb, BlockType::Water, BlockType::Lava}) {
        CAPTURE(static_cast<int>(t));
        CHECK_FALSE(Block(t).isSolid());
        CHECK_FALSE(Block(t).isOpaque());
    }
}

TEST_CASE("Vidrio y hojas: sólidos pero transparentes") {
    for (BlockType t : {BlockType::Glass, BlockType::Leaves}) {
        CAPTURE(static_cast<int>(t));
        CHECK(Block(t).isSolid());
        CHECK_FALSE(Block(t).isOpaque());
    }
}

TEST_CASE("Bloques normales: sólidos y opacos") {
    for (BlockType t : {BlockType::Grass, BlockType::Dirt, BlockType::Stone, BlockType::Wood,
                        BlockType::RefinedStone, BlockType::Brick, BlockType::TNT,
                        BlockType::Cobblestone, BlockType::Coal, BlockType::Sand,
                        BlockType::Gravel, BlockType::Log}) {
        CAPTURE(static_cast<int>(t));
        CHECK(Block(t).isSolid());
        CHECK(Block(t).isOpaque());
    }
}

// Atlas 512x512, tiles de 32 px -> 16 columnas; cada tile mide 1/16 en UV.
// El índice cuenta desde arriba a la izquierda y V está invertido (1 - fila/16).
static void checkTile(const UVRect& uv, int tile) {
    const float step = 1.0f / 16.0f;
    const int col = tile % 16;
    const int row = tile / 16;
    CHECK(uv.u_min == doctest::Approx(col * step));
    CHECK(uv.u_max == doctest::Approx((col + 1) * step));
    CHECK(uv.v_max == doctest::Approx(1.0f - row * step));
    CHECK(uv.v_min == doctest::Approx(1.0f - (row + 1) * step));
}

TEST_CASE("UVs del pasto por cara") {
    checkTile(Block::getUVs(BlockType::Grass, 4), 0); // Arriba
    checkTile(Block::getUVs(BlockType::Grass, 5), 2); // Abajo (tierra)
    for (int face = 0; face < 4; face++) {
        CAPTURE(face);
        checkTile(Block::getUVs(BlockType::Grass, face), 3); // Lados
    }
}

TEST_CASE("UVs de bloques con caras distintas y tiles lejanos") {
    checkTile(Block::getUVs(BlockType::Log, 4), 21);
    checkTile(Block::getUVs(BlockType::Log, 0), 20);
    checkTile(Block::getUVs(BlockType::Water, 0), 207);
    checkTile(Block::getUVs(BlockType::Lava, 0), 239);
}

TEST_CASE("Tipo sin tile usa el tile de debug 255") {
    checkTile(Block::getUVs(BlockType::Air, 0), 255);
}
