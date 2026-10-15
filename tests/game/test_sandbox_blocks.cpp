#include <doctest/doctest.h>
#include <game/SandboxBlocks.h>

#include <initializer_list>

using namespace mge;

namespace {
    struct Registered {
        BlockRegistry registry;
        sandbox::SandboxBlocks b = sandbox::registerSandboxBlocks(registry);
    };
}

TEST_CASE("Sandbox: 20 bloques registrados con nombre único") {
    Registered r;
    CHECK(r.registry.size() == 21); // + aire
    CHECK(r.registry.find("grass") == r.b.grass);
    CHECK(r.registry.find("lava") == r.b.lava);
}

TEST_CASE("Sandbox: atravesables = no sólidos y no opacos") {
    Registered r;
    for (BlockId id : {r.b.rose, r.b.flower, r.b.sapling, r.b.cobweb, r.b.water, r.b.lava}) {
        CAPTURE(r.registry.get(id).name);
        CHECK_FALSE(r.registry.isSolid(id));
        CHECK_FALSE(r.registry.isOpaque(id));
    }
}

TEST_CASE("Sandbox: vidrio y hojas sólidos pero transparentes; el resto sólido y opaco") {
    Registered r;
    for (BlockId id : {r.b.glass, r.b.leaves}) {
        CHECK(r.registry.isSolid(id));
        CHECK_FALSE(r.registry.isOpaque(id));
    }
    for (BlockId id : {r.b.grass, r.b.dirt, r.b.stone, r.b.planks, r.b.refinedStone, r.b.brick, r.b.tnt,
                       r.b.cobblestone, r.b.coal, r.b.sand, r.b.gravel, r.b.log}) {
        CAPTURE(r.registry.get(id).name);
        CHECK(r.registry.isSolid(id));
        CHECK(r.registry.isOpaque(id));
        CHECK(r.registry.get(id).shape == BlockShape::Cube);
    }
}

TEST_CASE("Sandbox: plantas en cruz") {
    Registered r;
    for (BlockId id : {r.b.rose, r.b.flower, r.b.sapling, r.b.cobweb})
        CHECK(r.registry.get(id).shape == BlockShape::Cross);
}

TEST_CASE("Sandbox: tiles del atlas por cara (los mismos que antes de mge_voxel)") {
    Registered r;
    const BlockDesc& grass = r.registry.get(r.b.grass);
    CHECK(grass.tile(BlockFace::Top) == 0);
    CHECK(grass.tile(BlockFace::Bottom) == 2);
    CHECK(grass.tile(BlockFace::Front) == 3);
    CHECK(r.registry.get(r.b.log).tile(BlockFace::Top) == 21);
    CHECK(r.registry.get(r.b.log).tile(BlockFace::Left) == 20);
    CHECK(r.registry.get(r.b.water).tile(BlockFace::Top) == sandbox::kWaterTile);
    CHECK(r.registry.get(r.b.lava).tile(BlockFace::Top) == sandbox::kLavaTile);
    CHECK(r.registry.get(r.b.stone).tile(BlockFace::Back) == 1);
}

TEST_CASE("Sandbox: todos los bloques tienen textura y color (los dos caminos)") {
    Registered r;
    for (std::size_t i = 1; i < r.registry.size(); i++) {
        const BlockDesc& desc = r.registry.get(static_cast<BlockId>(i));
        CAPTURE(desc.name);
        CHECK(desc.textured());
        CHECK(desc.color.a > 0.0f);
    }
}
