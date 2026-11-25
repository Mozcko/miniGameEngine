#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>

namespace platformer {

    // Cells of the ASCII levels in assets/levels (format and legend in assets/levels/README.md)
    enum class Tile : char {
        Empty,
        Ground,      // 'G': surface
        Dirt,        // 'D': beneath the surface
        BridgeLeft,  // '['
        Bridge,      // '-'
        BridgeRight, // ']'
        Pillar,      // '1', '2': supports in pits
        Ice,         // 'I'
        Snow,        // 'S': snowy surface
        SnowDirt,    // 's': beneath the snow
        Water,       // '~': non-solid; falling in sends you back to the start
    };

    enum class ItemKind : char {
        Coin,    // '?'
        Special, // '!'
    };

    struct Item {
        glm::ivec2 cell{0};
        ItemKind kind = ItemKind::Coin;
    };

    // A parsed level. Cells (x, y) with y = 0 at the top row, matching the file
    struct Level {
        std::string biome; // GRASS, SAND, RED_SAND, ICE…
        int width = 0;
        int height = 0;
        std::vector<Tile> tiles; // width × height, row by row
        glm::ivec2 spawn{0};     // '@'
        std::vector<Item> items;
        std::vector<glm::ivec2> enemies; // 'E'

        // Out of bounds: empty
        Tile at(int x, int y) const;
        bool solid(int x, int y) const; // Everything except empty and water
    };

    // Parses a level: "#BIOME <name>" followed by one row per line (shorter rows are padded with empty).
    // On invalid level (no rows, no '@', unknown characters) returns nullopt and sets error
    std::optional<Level> parseLevel(std::string_view text, std::string* error = nullptr);
    // Relative path → mge::assetPath (e.g., "levels/level1.txt")
    std::optional<Level> loadLevel(const std::filesystem::path& path, std::string* error = nullptr);
}
