#include <mge/voxel/Block.h>

#include <mge/core/Log.h>

namespace mge {

    BlockDesc& BlockDesc::setTile(int tile) {
        tiles.fill(tile);
        return *this;
    }

    BlockDesc& BlockDesc::setTiles(int top, int bottom, int sides) {
        tiles.fill(sides);
        tiles[static_cast<int>(BlockFace::Top)] = top;
        tiles[static_cast<int>(BlockFace::Bottom)] = bottom;
        return *this;
    }

    bool BlockDesc::textured() const {
        for (int tile : tiles)
            if (tile >= 0) return true;
        return false;
    }

    BlockRegistry::BlockRegistry() {
        BlockDesc air;
        air.name = "air";
        air.solid = false;
        air.opaque = false;
        air.shape = BlockShape::None;
        air.color = glm::vec4(0.0f);
        m_blocks.push_back(air);
    }

    BlockId BlockRegistry::add(BlockDesc desc) {
        MGE_ASSERT(m_blocks.size() < 0xFFFF, "BlockRegistry lleno");
        MGE_ASSERT(!find(desc.name).has_value(), "Nombre de bloque repetido: ", desc.name);
        m_blocks.push_back(std::move(desc));
        return static_cast<BlockId>(m_blocks.size() - 1);
    }

    const BlockDesc& BlockRegistry::get(BlockId id) const {
        MGE_ASSERT(id < m_blocks.size(), "BlockId desconocido: ", id);
        return id < m_blocks.size() ? m_blocks[id] : m_blocks[kAir];
    }

    std::optional<BlockId> BlockRegistry::find(const std::string& name) const {
        for (std::size_t i = 0; i < m_blocks.size(); i++)
            if (m_blocks[i].name == name) return static_cast<BlockId>(i);
        return std::nullopt;
    }

    UvRect TextureAtlas::tileUv(int tile) const {
        const int col = tile % columns;
        const int row = tile / columns;
        const float du = 1.0f / static_cast<float>(columns);
        const float dv = 1.0f / static_cast<float>(rows);
        // Las imágenes se cargan con la fila 0 arriba: V se invierte
        return {{col * du, 1.0f - (row + 1) * dv}, {(col + 1) * du, 1.0f - row * dv}};
    }
}
