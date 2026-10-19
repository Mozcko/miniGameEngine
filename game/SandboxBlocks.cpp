#include <game/SandboxBlocks.h>

namespace sandbox {

    namespace {
        mge::BlockDesc cube(const char* name, glm::vec3 color, int tile) {
            mge::BlockDesc desc;
            desc.name = name;
            desc.color = glm::vec4(color, 1.0f);
            desc.setTile(tile);
            return desc;
        }

        // Cubo que no tapa la vista (vidrio, hojas: sólidos; agua, lava: atravesables)
        mge::BlockDesc seeThrough(const char* name, glm::vec4 color, int tile, bool solid) {
            mge::BlockDesc desc;
            desc.name = name;
            desc.color = color;
            desc.solid = solid;
            desc.opaque = false;
            desc.setTile(tile);
            return desc;
        }

        // Plantas y telaraña: forma de cruz, atravesables, con agujeros (pasada cutout)
        mge::BlockDesc cross(const char* name, glm::vec3 color, int tile) {
            mge::BlockDesc desc = seeThrough(name, glm::vec4(color, 1.0f), tile, false);
            desc.shape = mge::BlockShape::Cross;
            desc.layer = mge::RenderLayer::Cutout;
            return desc;
        }
    }

    SandboxBlocks registerSandboxBlocks(mge::BlockRegistry& registry) {
        SandboxBlocks b{};
        b.grass = registry.add(cube("grass", {0.36f, 0.62f, 0.25f}, 3).setTiles(0, 2, 3));
        b.dirt = registry.add(cube("dirt", {0.47f, 0.33f, 0.21f}, 2));
        b.stone = registry.add(cube("stone", {0.50f, 0.50f, 0.52f}, 1));
        b.planks = registry.add(cube("planks", {0.70f, 0.55f, 0.33f}, 4));
        b.refinedStone = registry.add(cube("refined_stone", {0.62f, 0.62f, 0.62f}, 5).setTiles(6, 6, 5));
        b.brick = registry.add(cube("brick", {0.60f, 0.27f, 0.20f}, 7));
        b.tnt = registry.add(cube("tnt", {0.80f, 0.22f, 0.15f}, 8).setTiles(9, 10, 8));
        b.cobweb = registry.add(cross("cobweb", {0.90f, 0.90f, 0.92f}, 11));
        b.rose = registry.add(cross("rose", {0.80f, 0.10f, 0.12f}, 12));
        b.flower = registry.add(cross("flower", {0.95f, 0.90f, 0.30f}, 13));
        b.sapling = registry.add(cross("sapling", {0.30f, 0.55f, 0.20f}, 15));
        b.cobblestone = registry.add(cube("cobblestone", {0.45f, 0.45f, 0.45f}, 16));
        b.coal = registry.add(cube("coal", {0.20f, 0.20f, 0.22f}, 17));
        b.sand = registry.add(cube("sand", {0.86f, 0.80f, 0.56f}, 18));
        b.gravel = registry.add(cube("gravel", {0.53f, 0.50f, 0.48f}, 19));
        b.log = registry.add(cube("log", {0.40f, 0.30f, 0.18f}, 20).setTiles(21, 21, 20));
        b.glass = registry.add(seeThrough("glass", {0.80f, 0.90f, 0.95f, 0.35f}, 49, true));
        mge::BlockDesc leaves = seeThrough("leaves", {0.22f, 0.50f, 0.18f, 1.0f}, 52, true);
        leaves.layer = mge::RenderLayer::Cutout; // Agujeros, no semitransparencia (KI#21)
        b.leaves = registry.add(leaves);
        b.water = registry.add(seeThrough("water", {0.20f, 0.40f, 0.85f, 0.6f}, kWaterTile, false));
        b.lava = registry.add(seeThrough("lava", {0.95f, 0.40f, 0.05f, 1.0f}, kLavaTile, false));
        return b;
    }
}
