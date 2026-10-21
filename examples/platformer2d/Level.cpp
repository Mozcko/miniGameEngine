#include <examples/platformer2d/Level.h>

#include <mge/core/Assets.h>

#include <algorithm>
#include <fstream>
#include <sstream>

namespace platformer {

    Tile Level::at(int x, int y) const {
        if (x < 0 || y < 0 || x >= width || y >= height) return Tile::Empty;
        return tiles[std::size_t(y) * std::size_t(width) + std::size_t(x)];
    }

    bool Level::solid(int x, int y) const {
        const Tile tile = at(x, y);
        return tile != Tile::Empty && tile != Tile::Water;
    }

    namespace {
        bool fail(std::string* error, std::string message) {
            if (error) *error = std::move(message);
            return false;
        }

        // Una casilla del archivo. false = carácter desconocido
        bool readCell(char c, int x, int y, Level& level) {
            Tile tile = Tile::Empty;
            switch (c) {
            case '.': break;
            case 'G': tile = Tile::Ground; break;
            case 'D': tile = Tile::Dirt; break;
            case '[': tile = Tile::BridgeLeft; break;
            case '-': tile = Tile::Bridge; break;
            case ']': tile = Tile::BridgeRight; break;
            case '1':
            case '2': tile = Tile::Pillar; break;
            case 'I': tile = Tile::Ice; break;
            case 'S': tile = Tile::Snow; break;
            case 's': tile = Tile::SnowDirt; break;
            case '~': tile = Tile::Water; break;
            case '@': level.spawn = {x, y}; break;
            case '?': level.items.push_back({{x, y}, ItemKind::Coin}); break;
            case '!': level.items.push_back({{x, y}, ItemKind::Special}); break;
            case 'E': level.enemies.push_back({x, y}); break;
            default: return false;
            }
            level.tiles[std::size_t(y) * std::size_t(level.width) + std::size_t(x)] = tile;
            return true;
        }
    }

    std::optional<Level> parseLevel(std::string_view text, std::string* error) {
        std::vector<std::string_view> lines;
        for (std::size_t start = 0; start <= text.size();) {
            std::size_t end = text.find('\n', start);
            if (end == std::string_view::npos) end = text.size();
            std::string_view line = text.substr(start, end - start);
            if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
            lines.push_back(line);
            start = end + 1;
        }
        while (!lines.empty() && lines.back().empty()) lines.pop_back();

        Level level;
        constexpr std::string_view kBiome = "#BIOME ";
        if (!lines.empty() && lines.front().substr(0, kBiome.size()) == kBiome) {
            level.biome = std::string(lines.front().substr(kBiome.size()));
            lines.erase(lines.begin());
        }
        if (lines.empty()) {
            fail(error, "el nivel no tiene filas");
            return std::nullopt;
        }
        level.height = int(lines.size());
        for (std::string_view line : lines) level.width = std::max(level.width, int(line.size()));
        level.tiles.assign(std::size_t(level.width) * std::size_t(level.height), Tile::Empty);

        bool hasSpawn = false;
        for (int y = 0; y < level.height; y++)
            for (int x = 0; x < int(lines[std::size_t(y)].size()); x++) {
                const char c = lines[std::size_t(y)][std::size_t(x)];
                if (c == '@') hasSpawn = true;
                if (!readCell(c, x, y, level)) {
                    fail(error, std::string("carácter desconocido '") + c + "' en la fila " +
                                    std::to_string(y + 1) + ", columna " + std::to_string(x + 1));
                    return std::nullopt;
                }
            }
        if (!hasSpawn) {
            fail(error, "el nivel no tiene '@' (inicio del jugador)");
            return std::nullopt;
        }
        return level;
    }

    std::optional<Level> loadLevel(const std::filesystem::path& path, std::string* error) {
        const std::filesystem::path full = path.is_absolute() ? path : mge::assetPath(path.generic_string());
        std::ifstream file(full);
        if (!file) {
            fail(error, "no se pudo abrir " + full.string());
            return std::nullopt;
        }
        std::stringstream text;
        text << file.rdbuf();
        return parseLevel(text.str(), error);
    }
}
