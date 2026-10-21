#include <examples/platformer2d/PlatformerView.h>

#include <algorithm>
#include <cmath>

namespace platformer {

    Palette paletteFor(const std::string& biome) {
        if (biome == "SAND")
            return {{0.98f, 0.85f, 0.6f, 1},
                    {0.93f, 0.8f, 0.45f, 1},
                    {0.8f, 0.62f, 0.35f, 1},
                    {0.55f, 0.4f, 0.25f, 1}};
        if (biome == "RED_SAND")
            return {{0.95f, 0.65f, 0.5f, 1},
                    {0.85f, 0.4f, 0.25f, 1},
                    {0.6f, 0.28f, 0.18f, 1},
                    {0.45f, 0.3f, 0.2f, 1}};
        if (biome == "ICE")
            return {{0.75f, 0.88f, 1.0f, 1},
                    {0.85f, 0.95f, 1.0f, 1},
                    {0.5f, 0.65f, 0.85f, 1},
                    {0.4f, 0.5f, 0.65f, 1}};
        return {
            {0.5f, 0.75f, 1.0f, 1}, {0.3f, 0.75f, 0.3f, 1}, {0.5f, 0.33f, 0.2f, 1}, {0.55f, 0.38f, 0.22f, 1}};
    }

    float viewScale(const Level& level, std::uint32_t framebufferHeight) {
        const float levelHeight = float(level.height + 2) * kTilePixels;
        return std::max(1.0f, std::floor(float(framebufferHeight) / levelHeight));
    }

    namespace {
        constexpr float kTile = kTilePixels;

        void drawTile(mge::Renderer2D& ui, Tile tile, const glm::vec2& p, const Palette& palette) {
            switch (tile) {
            case Tile::Ground:
                ui.rect(p, {kTile, kTile}, palette.dirt);
                ui.rect(p, {kTile, 8.0f}, palette.ground);
                break;
            case Tile::Dirt:
                ui.rect(p, {kTile, kTile}, palette.dirt * glm::vec4(0.85f, 0.85f, 0.85f, 1));
                break;
            case Tile::BridgeLeft:
            case Tile::Bridge:
            case Tile::BridgeRight:
                ui.rect(p, {kTile, 10.0f}, palette.bridge);
                if (tile != Tile::Bridge) ui.rect(p + glm::vec2(12.0f, 10.0f), {8.0f, 22.0f}, palette.bridge);
                break;
            case Tile::Pillar: ui.rect(p + glm::vec2(4.0f, 0.0f), {24.0f, kTile}, palette.bridge); break;
            case Tile::Ice:
                ui.rect(p, {kTile, kTile}, {0.65f, 0.85f, 0.98f, 1.0f});
                ui.line(p + glm::vec2(6.0f, 24.0f), p + glm::vec2(24.0f, 6.0f), {1, 1, 1, 0.8f}, 2.0f);
                break;
            case Tile::Snow:
                ui.rect(p, {kTile, kTile}, palette.dirt);
                ui.rect(p, {kTile, 10.0f}, {0.97f, 0.98f, 1.0f, 1.0f});
                break;
            case Tile::SnowDirt: ui.rect(p, {kTile, kTile}, palette.dirt); break;
            case Tile::Water:
                ui.rect(p + glm::vec2(0.0f, 6.0f), {kTile, kTile - 6.0f}, {0.15f, 0.45f, 0.9f, 0.75f});
                break;
            case Tile::Empty: break;
            }
        }
    }

    void drawScene(mge::Renderer2D& ui, const Platformer& game, int levelNumber) {
        const Level& level = game.level();
        const Palette palette = paletteFor(level.biome);
        const glm::vec2 view = ui.size();
        ui.rect({0.0f, 0.0f}, view, palette.sky);

        // Cámara: sigue al jugador en x, sin salirse del nivel; el nivel centrado en y
        const glm::vec2 player = game.playerPosition() * kTile;
        const float maxX = std::max(0.0f, float(level.width) * kTile - view.x);
        const glm::vec2 camera(std::clamp(player.x - view.x * 0.5f, 0.0f, maxX),
                               -std::round((view.y - float(level.height) * kTile) * 0.5f));
        const auto toScreen = [&](const glm::vec2& cell) { return glm::round(cell * kTile - camera); };

        // Solo las columnas que se ven
        const int firstColumn = std::max(0, int(camera.x / kTile));
        const int lastColumn = std::min(level.width - 1, int((camera.x + view.x) / kTile));
        for (int y = 0; y < level.height; y++)
            for (int x = firstColumn; x <= lastColumn; x++)
                drawTile(ui, level.at(x, y), toScreen(glm::vec2(float(x), float(y))), palette);

        for (std::size_t i = 0; i < level.items.size(); i++) {
            if (game.collected(i)) continue;
            const glm::vec2 p = toScreen(glm::vec2(level.items[i].cell));
            const bool coin = level.items[i].kind == ItemKind::Coin;
            ui.rect(p + glm::vec2(6.0f), {20.0f, 20.0f},
                    coin ? glm::vec4(1.0f, 0.85f, 0.1f, 1.0f) : glm::vec4(0.9f, 0.3f, 0.9f, 1.0f));
            ui.rectOutline(p + glm::vec2(6.0f), {20.0f, 20.0f}, {0.4f, 0.25f, 0.0f, 1.0f}, 2.0f);
            ui.text(p + glm::vec2(12.0f), coin ? "?" : "!", {0.3f, 0.15f, 0.0f, 1.0f});
        }
        for (const glm::ivec2& cell : level.enemies) {
            const glm::vec2 p = toScreen(glm::vec2(cell));
            ui.rect(p + glm::vec2(5.0f, 10.0f), {22.0f, 22.0f}, {0.85f, 0.15f, 0.15f, 1.0f});
            ui.rect(p + glm::vec2(9.0f, 15.0f), {4.0f, 4.0f}, {1, 1, 1, 1});
            ui.rect(p + glm::vec2(19.0f, 15.0f), {4.0f, 4.0f}, {1, 1, 1, 1});
        }
        const glm::vec2 size = Platformer::kPlayerSize * kTile;
        ui.rect(toScreen(game.playerPosition()), size, {0.15f, 0.35f, 0.95f, 1.0f});
        ui.rectOutline(toScreen(game.playerPosition()), size, {0.05f, 0.1f, 0.3f, 1.0f}, 2.0f);

        // HUD
        ui.rect({0.0f, 0.0f}, {view.x, 22.0f}, {0.0f, 0.0f, 0.0f, 0.45f});
        ui.text({8.0f, 7.0f},
                "Nivel " + std::to_string(levelNumber) + " (" + level.biome + ")   monedas " +
                    std::to_string(game.coins()) + "   caidas " + std::to_string(game.deaths()),
                {1.0f, 1.0f, 1.0f, 1.0f});
        ui.text({8.0f, view.y - 14.0f}, "A/D mover  Espacio saltar  1-4 nivel  F1 rendimiento",
                {0.0f, 0.0f, 0.0f, 0.7f});
    }
}
