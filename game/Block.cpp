#include <game/Block.h>

namespace mge {

    Block::Block(BlockType type) : m_type(type) {}

    bool Block::isActive() const {
        return m_type != BlockType::Air;
    }

    bool Block::isSolid() const {
        switch (m_type) {
        case BlockType::Air:
        case BlockType::Rose:
        case BlockType::Flower:
        case BlockType::Sapling:
        case BlockType::Cobweb:
        case BlockType::Water:
        case BlockType::Lava: return false; // Se pueden atravesar
        default: return true;               // Sólidos
        }
    }

    bool Block::isOpaque() const {
        switch (m_type) {
        case BlockType::Air:
        case BlockType::Rose:
        case BlockType::Flower:
        case BlockType::Sapling:
        case BlockType::Cobweb:
        case BlockType::Water:
        case BlockType::Lava:
        case BlockType::Glass:
        case BlockType::Leaves: return false; // Permiten ver a través de ellos
        default: return true;                 // Bloquean la vista completamente
        }
    }

    UVRect Block::getUVs(BlockType type, int faceIndex) {
        // Configuración del Tileset (Hardcoded por ahora para simplificar)
        const int texW = 512;
        const int texH = 512;
        const int tileS = 32;
        const int cols = texW / tileS;

        int tileIndex = 0;

        // Mapeo de Tipo de Bloque a Índice de Tile
        switch (type) {
        case BlockType::Grass:
            if (faceIndex == 4)
                tileIndex = 0; // Top
            else if (faceIndex == 5)
                tileIndex = 2; // Bottom
            else
                tileIndex = 3; // Sides
            break;
        case BlockType::Dirt: tileIndex = 2; break;
        case BlockType::Stone: tileIndex = 1; break;
        case BlockType::Wood: tileIndex = 4; break;
        case BlockType::RefinedStone:
            if (faceIndex == 4 || faceIndex == 5)
                tileIndex = 6; // Top/Bottom
            else
                tileIndex = 5; // Side
            break;
        case BlockType::Brick: tileIndex = 7; break;
        case BlockType::TNT:
            if (faceIndex == 4)
                tileIndex = 9; // Top
            else if (faceIndex == 5)
                tileIndex = 10; // Bottom
            else
                tileIndex = 8; // Side
            break;
        case BlockType::Cobweb: tileIndex = 11; break;
        case BlockType::Rose: tileIndex = 12; break;
        case BlockType::Flower: tileIndex = 13; break;
        case BlockType::Sapling: tileIndex = 15; break;
        case BlockType::Cobblestone: tileIndex = 16; break;
        case BlockType::Coal: tileIndex = 17; break;
        case BlockType::Sand: tileIndex = 18; break;
        case BlockType::Gravel: tileIndex = 19; break;
        case BlockType::Log:
            if (faceIndex == 4 || faceIndex == 5)
                tileIndex = 21; // Top/Bottom (Anillos)
            else
                tileIndex = 20; // Side (Corteza)
            break;
        case BlockType::Glass: tileIndex = 49; break;
        case BlockType::Leaves: tileIndex = 52; break;
        case BlockType::Water: tileIndex = 207; break;
        case BlockType::Lava: tileIndex = 239; break;
        default:
            tileIndex = 255; // Debug / Error
            break;
        }

        // Cálculo de UVs (Copiado de la lógica anterior)
        int col = tileIndex % cols;
        int row = tileIndex / cols;

        float uMin = (float)(col * tileS) / texW;
        float uMax = (float)((col + 1) * tileS) / texW;

        // Invertimos Y
        float vMax = 1.0f - (float)(row * tileS) / texH;
        float vMin = 1.0f - (float)((row + 1) * tileS) / texH;

        return {uMin, vMin, uMax, vMax};
    }
}