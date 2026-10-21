#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>

namespace platformer {

    // Casillas de los niveles ASCII de assets/levels (formato y leyenda en assets/levels/README.md)
    enum class Tile : char {
        Empty,
        Ground,      // 'G': superficie
        Dirt,        // 'D': debajo de la superficie
        BridgeLeft,  // '['
        Bridge,      // '-'
        BridgeRight, // ']'
        Pillar,      // '1', '2': apoyos en el pozo
        Ice,         // 'I'
        Snow,        // 'S': superficie nevada
        SnowDirt,    // 's': debajo de la nieve
        Water,       // '~': no es sólida; caer adentro te devuelve al inicio
    };

    enum class ItemKind : char {
        Coin,    // '?'
        Special, // '!'
    };

    struct Item {
        glm::ivec2 cell{0};
        ItemKind kind = ItemKind::Coin;
    };

    // Un nivel ya leído. Celdas (x, y) con y = 0 en la fila de arriba, como en el archivo
    struct Level {
        std::string biome; // GRASS, SAND, RED_SAND, ICE…
        int width = 0;
        int height = 0;
        std::vector<Tile> tiles; // width × height, fila por fila
        glm::ivec2 spawn{0};     // '@'
        std::vector<Item> items;
        std::vector<glm::ivec2> enemies; // 'E'

        // Fuera del nivel: vacío
        Tile at(int x, int y) const;
        bool solid(int x, int y) const; // Todo menos vacío y agua
    };

    // Lee un nivel: "#BIOME <nombre>" y después una fila por línea (las más cortas se completan con vacío).
    // Sin nivel válido (sin filas, sin '@', caracteres desconocidos) devuelve nullopt y el motivo en error
    std::optional<Level> parseLevel(std::string_view text, std::string* error = nullptr);
    // path relativo → mge::assetPath (ej: "levels/level1.txt")
    std::optional<Level> loadLevel(const std::filesystem::path& path, std::string* error = nullptr);
}
