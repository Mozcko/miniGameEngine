#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace mge {

    // Id de bloque dentro de un BlockRegistry. 0 es siempre aire.
    using BlockId = std::uint16_t;
    inline constexpr BlockId kAir = 0;

    // Caras de un cubo (mismo orden que el atlas del sandbox)
    enum class BlockFace : std::uint8_t {
        Front = 0,  // +Z
        Back = 1,   // -Z
        Left = 2,   // -X
        Right = 3,  // +X
        Top = 4,    // +Y
        Bottom = 5, // -Y
    };
    inline constexpr int kBlockFaceCount = 6;

    enum class BlockShape : std::uint8_t {
        None,  // No se dibuja (aire)
        Cube,  // Cubo de 6 caras (se ocultan las caras tapadas)
        Cross, // Dos planos diagonales (flores, pasto alto)
    };

    // Descripción de un tipo de bloque. Ambos caminos (ADR 0004):
    //   - procedural: `color` (color por vértice)
    //   - asset: `tiles`, un tile del atlas por cara (-1 = sin textura)
    // Pueden combinarse: con textura, el color se multiplica (blanco = textura tal cual).
    struct BlockDesc {
        std::string name;
        bool solid = true;  // Colisiona
        bool opaque = true; // Tapa la vista (oculta las caras vecinas)
        BlockShape shape = BlockShape::Cube;
        glm::vec4 color = glm::vec4(1.0f);
        std::array<int, kBlockFaceCount> tiles = {-1, -1, -1, -1, -1, -1};

        BlockDesc& setTile(int tile);                        // Todas las caras
        BlockDesc& setTiles(int top, int bottom, int sides); // Arriba / abajo / costados
        bool textured() const;
        int tile(BlockFace face) const { return tiles[static_cast<int>(face)]; }
    };

    // Catálogo de bloques del juego. El id 0 (aire: ni sólido ni opaco, sin forma) ya viene registrado.
    class BlockRegistry {
    public:
        BlockRegistry();

        BlockId add(BlockDesc desc); // Devuelve el id nuevo (consecutivo)
        // Id desconocido → assert en debug; en release devuelve el aire
        const BlockDesc& get(BlockId id) const;
        std::optional<BlockId> find(const std::string& name) const;
        std::size_t size() const { return m_blocks.size(); }

        bool isSolid(BlockId id) const { return get(id).solid; }
        bool isOpaque(BlockId id) const { return get(id).opaque; }

    private:
        std::vector<BlockDesc> m_blocks;
    };

    // Rectángulo UV de un tile en un atlas de columns × rows (tile 0 arriba a la izquierda, V hacia arriba)
    struct UvRect {
        glm::vec2 min;
        glm::vec2 max;
    };
    struct TextureAtlas {
        int columns = 16;
        int rows = 16;
        UvRect tileUv(int tile) const;
    };
}
