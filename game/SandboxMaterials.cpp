#include <game/SandboxMaterials.h>

#include <game/SandboxBlocks.h>
#include <mge/core/Random.h>
#include <mge/procgen/TextureGen.h>

#include <functional>
#include <utility>

namespace sandbox {

    using mge::texgen::Palette;
    using mge::texgen::Rect;
    namespace tg = mge::texgen;

    namespace {
        glm::vec4 rgb(float r, float g, float b) {
            return {r, g, b, 1.0f};
        }

        const Palette kDirt({{0.0f, rgb(0.33f, 0.22f, 0.13f)},
                             {0.6f, rgb(0.47f, 0.33f, 0.21f)},
                             {1.0f, rgb(0.55f, 0.40f, 0.26f)}});
        const Palette kGrass({{0.0f, rgb(0.24f, 0.48f, 0.16f)},
                              {0.5f, rgb(0.36f, 0.62f, 0.25f)},
                              {1.0f, rgb(0.48f, 0.72f, 0.30f)}});
        const Palette kStone({{0.0f, rgb(0.38f, 0.38f, 0.40f)},
                              {0.5f, rgb(0.50f, 0.50f, 0.52f)},
                              {1.0f, rgb(0.62f, 0.62f, 0.64f)}});
        const Palette kSand(rgb(0.80f, 0.73f, 0.50f), rgb(0.93f, 0.87f, 0.63f));
        const Palette kGravel({{0.0f, rgb(0.35f, 0.33f, 0.32f)},
                               {0.5f, rgb(0.53f, 0.50f, 0.48f)},
                               {1.0f, rgb(0.70f, 0.67f, 0.64f)}});
        const Palette kWood(rgb(0.55f, 0.40f, 0.22f), rgb(0.78f, 0.62f, 0.38f));
        const Palette kBark(rgb(0.25f, 0.17f, 0.09f), rgb(0.45f, 0.32f, 0.18f));
        const Palette kRings(rgb(0.60f, 0.45f, 0.26f), rgb(0.80f, 0.66f, 0.42f));
        const Palette kLeaves(rgb(0.13f, 0.36f, 0.10f), rgb(0.30f, 0.58f, 0.22f));
        const Palette kBrick(rgb(0.50f, 0.20f, 0.14f), rgb(0.72f, 0.34f, 0.24f));
        const Palette kTnt(rgb(0.70f, 0.12f, 0.08f), rgb(0.88f, 0.22f, 0.14f));

        // Agua y lava: el mismo ruido con otra semilla por cuadro = animación
        void water(mge::Image& image, const Rect& rect, std::uint32_t seed) {
            tg::noise(image, rect,
                      Palette({{0.0f, {0.10f, 0.28f, 0.70f, 0.75f}}, {1.0f, {0.30f, 0.55f, 0.95f, 0.75f}}}),
                      3.0f, seed, 2);
        }
        void lava(mge::Image& image, const Rect& rect, std::uint32_t seed) {
            tg::noise(image, rect,
                      Palette({{0.0f, rgb(0.55f, 0.08f, 0.02f)},
                               {0.6f, rgb(0.95f, 0.40f, 0.05f)},
                               {1.0f, rgb(1.0f, 0.85f, 0.30f)}}),
                      4.0f, seed, 3);
        }

        using Painter = std::function<void(mge::Image&, const Rect&, std::uint32_t)>;

        std::vector<std::pair<int, Painter>> painters() {
            std::vector<std::pair<int, Painter>> p;
            p.emplace_back(
                0, [](mge::Image& im, const Rect& r, std::uint32_t s) { tg::noise(im, r, kGrass, 6.0f, s); });
            p.emplace_back(1, [](mge::Image& im, const Rect& r, std::uint32_t s) {
                tg::noise(im, r, kStone, 5.0f, s);
                tg::speckle(im, r, rgb(0.34f, 0.34f, 0.36f), 0.04f, s + 1);
            });
            p.emplace_back(2, [](mge::Image& im, const Rect& r, std::uint32_t s) {
                tg::noise(im, r, kDirt, 6.0f, s);
                tg::speckle(im, r, rgb(0.30f, 0.20f, 0.12f), 0.06f, s + 1);
            });
            p.emplace_back(3, [](mge::Image& im, const Rect& r, std::uint32_t s) { // Costado del pasto
                tg::noise(im, r, kDirt, 6.0f, s);
                Rect top = r;
                top.height = r.height / 4;
                tg::noise(im, top, kGrass, 6.0f, s + 2);
                for (int u = 0; u < r.width; u++) // Borde irregular del pasto
                    if (mge::hash2i(u, 0, s) % 3 == 0) im.set(r.x + u, r.y + top.height, kGrass.sample(0.4f));
            });
            p.emplace_back(
                4, [](mge::Image& im, const Rect& r, std::uint32_t s) { tg::planks(im, r, kWood, 4, s); });
            p.emplace_back(5, [](mge::Image& im, const Rect& r, std::uint32_t s) { // Piedra refinada: lados
                tg::noise(im, r, kStone, 3.0f, s, 2);
                tg::frame(im, r, rgb(0.42f, 0.42f, 0.44f), 2);
            });
            p.emplace_back(6, [](mge::Image& im, const Rect& r, std::uint32_t s) { // Piedra refinada: arriba
                tg::noise(im, r, Palette(rgb(0.58f, 0.58f, 0.60f), rgb(0.68f, 0.68f, 0.70f)), 2.0f, s, 2);
                tg::frame(im, r, rgb(0.44f, 0.44f, 0.46f), 3);
            });
            p.emplace_back(7, [](mge::Image& im, const Rect& r, std::uint32_t s) {
                tg::bricks(im, r, kBrick, rgb(0.72f, 0.70f, 0.66f), 4, 2, 2, s);
            });
            p.emplace_back(8, [](mge::Image& im, const Rect& r, std::uint32_t s) { // TNT: costado
                tg::noise(im, r, kTnt, 3.0f, s, 2);
                Rect band = r;
                band.y = r.y + r.height * 3 / 8;
                band.height = r.height / 4;
                tg::noise(im, band, Palette(rgb(0.85f, 0.85f, 0.82f), rgb(0.95f, 0.95f, 0.92f)), 2.0f, s + 1,
                          1);
            });
            p.emplace_back(9, [](mge::Image& im, const Rect& r, std::uint32_t s) {
                tg::noise(im, r, kTnt, 3.0f, s, 2);
                tg::rings(im, Rect{r.x + r.width / 3, r.y + r.height / 3, r.width / 3, r.height / 3}, kBark,
                          3.0f, s);
            });
            p.emplace_back(10, [](mge::Image& im, const Rect& r, std::uint32_t s) {
                tg::noise(im, r, kTnt, 3.0f, s, 2);
            });
            p.emplace_back(
                11, [](mge::Image& im, const Rect& r, std::uint32_t s) { // Telaraña: hilos sobre transparente
                    tg::plant(im, r, {0.0f, 0.0f, 0.0f, 0.0f}, {0.92f, 0.92f, 0.95f, 1.0f}, 0, s);
                    for (int i = 0; i < r.width; i++) {
                        im.set(r.x + i, r.y + i, {0.9f, 0.9f, 0.92f, 1.0f});
                        im.set(r.x + r.width - 1 - i, r.y + i, {0.9f, 0.9f, 0.92f, 1.0f});
                        im.set(r.x + i, r.y + r.height / 2, {0.9f, 0.9f, 0.92f, 1.0f});
                        im.set(r.x + r.width / 2, r.y + i, {0.9f, 0.9f, 0.92f, 1.0f});
                    }
                });
            p.emplace_back(12, [](mge::Image& im, const Rect& r, std::uint32_t s) {
                tg::plant(im, r, rgb(0.20f, 0.50f, 0.15f), rgb(0.85f, 0.10f, 0.12f), 5, s); // Rosa
            });
            p.emplace_back(13, [](mge::Image& im, const Rect& r, std::uint32_t s) {
                tg::plant(im, r, rgb(0.20f, 0.50f, 0.15f), rgb(0.97f, 0.92f, 0.30f), 4, s); // Margarita
            });
            p.emplace_back(15, [](mge::Image& im, const Rect& r, std::uint32_t s) {
                tg::plant(im, r, rgb(0.30f, 0.45f, 0.18f), rgb(0.28f, 0.60f, 0.22f), 7, s); // Brote
            });
            p.emplace_back(16, [](mge::Image& im, const Rect& r, std::uint32_t s) { // Guijarro
                tg::bricks(im, r, kStone, rgb(0.30f, 0.30f, 0.32f), 5, 3, 1, s);
                tg::speckle(im, r, rgb(0.40f, 0.40f, 0.42f), 0.08f, s + 1);
            });
            p.emplace_back(17, [](mge::Image& im, const Rect& r, std::uint32_t s) { // Carbón
                tg::noise(im, r, kStone, 5.0f, s);
                tg::ore(im, r, rgb(0.10f, 0.10f, 0.11f), 6, 2, s + 1);
            });
            p.emplace_back(18, [](mge::Image& im, const Rect& r, std::uint32_t s) {
                tg::noise(im, r, kSand, 8.0f, s, 2);
            });
            p.emplace_back(19, [](mge::Image& im, const Rect& r, std::uint32_t s) {
                tg::noise(im, r, kGravel, 10.0f, s, 2);
                tg::speckle(im, r, rgb(0.30f, 0.28f, 0.27f), 0.10f, s + 1);
            });
            p.emplace_back(
                20, [](mge::Image& im, const Rect& r, std::uint32_t s) { tg::stripes(im, r, kBark, 4, s); });
            p.emplace_back(21, [](mge::Image& im, const Rect& r, std::uint32_t s) {
                tg::rings(im, r, kRings, 4.0f, s);
                tg::frame(im, r, kBark.sample(0.4f), 2);
            });
            p.emplace_back(
                49, [](mge::Image& im, const Rect& r, std::uint32_t) { // Vidrio: casi transparente con marco
                    for (int v = 0; v < r.height; v++)
                        for (int u = 0; u < r.width; u++)
                            im.set(r.x + u, r.y + v, {0.80f, 0.90f, 0.95f, 0.15f});
                    tg::frame(im, r, {0.85f, 0.92f, 0.97f, 0.9f}, 2);
                    for (int i = 4; i < 12; i++)
                        im.set(r.x + i + 4, r.y + i, {1.0f, 1.0f, 1.0f, 0.6f}); // Reflejo
                });
            p.emplace_back(52, [](mge::Image& im, const Rect& r, std::uint32_t s) {
                tg::leaves(im, r, kLeaves, 0.25f, s);
            });
            for (std::size_t i = 0; i < kWaterFrames.size(); i++)
                p.emplace_back(kWaterFrames[i], [i](mge::Image& im, const Rect& r, std::uint32_t s) {
                    water(im, r, s + std::uint32_t(i) * 101);
                });
            for (std::size_t i = 0; i < kLavaFrames.size(); i++)
                p.emplace_back(kLavaFrames[i], [i](mge::Image& im, const Rect& r, std::uint32_t s) {
                    lava(im, r, s + std::uint32_t(i) * 131);
                });
            return p;
        }
    }

    const std::vector<int>& proceduralTiles() {
        static const std::vector<int> kTiles = [] {
            std::vector<int> result;
            for (const auto& [tile, painter] : painters()) result.push_back(tile);
            return result;
        }();
        return kTiles;
    }

    mge::Image buildProceduralAtlas(std::uint32_t seed) {
        mge::Image atlas(kAtlasColumns * kAtlasTileSize, kAtlasRows * kAtlasTileSize,
                         glm::vec4(1.0f, 0.0f, 1.0f, 1.0f));
        for (const auto& [tile, painter] : painters())
            painter(atlas, tg::tileRect(tile, kAtlasColumns, kAtlasTileSize),
                    seed + std::uint32_t(tile) * 7919u);
        return atlas;
    }
}
