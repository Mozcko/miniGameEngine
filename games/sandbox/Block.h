#pragma once
#include <glm/glm.hpp>

namespace mge {

    enum class BlockType {
        Air = 0,
        Grass,
        Dirt,
        Stone,
        Wood,         // Madera refinada
        RefinedStone, // Piedra refinada
        Brick,        // Ladrillo
        TNT,
        Cobweb,       // Telaraña
        Rose,
        Flower,       // Margarita
        Sapling,
        Cobblestone,  // Guijarro
        Coal,         // Carbon (bloque)
        Sand,
        Gravel,
        Log,          // Tronco de arbol
        Glass,
        Leaves,
        Water,
        Lava
    };

    struct UVRect {
        float uMin, vMin, uMax, vMax;
    };

    class Block {
    public:
        Block(BlockType type = BlockType::Air);

        bool isActive() const;
        bool isSolid() const;  // Define si el jugador choca con el bloque
        bool isOpaque() const; // Define si el bloque bloquea la visión (culling)
        BlockType getType() const { return m_type; }

        // Obtiene las UVs para una cara específica (0:Front, 1:Back, 2:Left, 3:Right, 4:Top, 5:Bottom)
        static UVRect getUVs(BlockType type, int faceIndex);

    private:
        BlockType m_type;
    };
}