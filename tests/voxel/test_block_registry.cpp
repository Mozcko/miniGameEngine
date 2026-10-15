#include <doctest/doctest.h>
#include <mge/voxel/Block.h>

using namespace mge;

TEST_CASE("BlockRegistry: el aire viene registrado como id 0") {
    BlockRegistry registry;
    CHECK(registry.size() == 1);
    CHECK(registry.get(kAir).name == "air");
    CHECK_FALSE(registry.isSolid(kAir));
    CHECK_FALSE(registry.isOpaque(kAir));
    CHECK(registry.get(kAir).shape == BlockShape::None);
}

TEST_CASE("BlockRegistry: ids consecutivos, búsqueda por nombre") {
    BlockRegistry registry;
    BlockDesc stone;
    stone.name = "stone";
    BlockDesc water;
    water.name = "water";
    water.solid = false;
    water.opaque = false;

    const BlockId s = registry.add(stone);
    const BlockId w = registry.add(water);
    CHECK(s == 1);
    CHECK(w == 2);
    CHECK(registry.isSolid(s));
    CHECK(registry.isOpaque(s));
    CHECK_FALSE(registry.isSolid(w));
    CHECK(registry.find("water") == w);
    CHECK_FALSE(registry.find("lava").has_value());
}

TEST_CASE("BlockDesc: color, tiles por cara o ambos") {
    BlockDesc colorOnly;
    CHECK_FALSE(colorOnly.textured());

    BlockDesc grass;
    grass.setTiles(0, 2, 3);
    CHECK(grass.textured());
    CHECK(grass.tile(BlockFace::Top) == 0);
    CHECK(grass.tile(BlockFace::Bottom) == 2);
    for (BlockFace f : {BlockFace::Front, BlockFace::Back, BlockFace::Left, BlockFace::Right})
        CHECK(grass.tile(f) == 3);

    grass.setTile(7);
    for (int f = 0; f < kBlockFaceCount; f++) CHECK(grass.tiles[f] == 7);
}

TEST_CASE("TextureAtlas: UV de un tile, fila 0 arriba") {
    TextureAtlas atlas{16, 16};
    const float step = 1.0f / 16.0f;
    for (int tile : {0, 3, 21, 207, 255}) {
        CAPTURE(tile);
        const UvRect uv = atlas.tileUv(tile);
        const int col = tile % 16;
        const int row = tile / 16;
        CHECK(uv.min.x == doctest::Approx(col * step));
        CHECK(uv.max.x == doctest::Approx((col + 1) * step));
        CHECK(uv.max.y == doctest::Approx(1.0f - row * step));
        CHECK(uv.min.y == doctest::Approx(1.0f - (row + 1) * step));
    }

    // Atlas no cuadrado
    const UvRect uv = TextureAtlas{4, 2}.tileUv(5); // Columna 1, fila 1
    CHECK(uv.min.x == doctest::Approx(0.25f));
    CHECK(uv.max.x == doctest::Approx(0.5f));
    CHECK(uv.min.y == doctest::Approx(0.0f));
    CHECK(uv.max.y == doctest::Approx(0.5f));
}
