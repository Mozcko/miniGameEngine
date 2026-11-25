#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace mge {

    // Block ID within a BlockRegistry. 0 is always air.
    using BlockId = std::uint16_t;
    inline constexpr BlockId kAir = 0;

    // Faces of a cube (same order as the sandbox atlas)
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
        None,  // Not drawn (air)
        Cube,  // 6-faced cube (occluded faces are hidden)
        Cross, // Two diagonal planes (flowers, tall grass)
    };

    // Render pass in which a block is drawn
    enum class RenderLayer : std::uint8_t {
        Opaque,      // Visually solid
        Cutout,      // With holes (leaves, plants): transparent pixels discarded, writes depth
        Translucent, // Semi-transparent (water, glass): alpha blending, sorted back-to-front
    };

    // Description of a block type. Both paths (ADR 0004):
    //   - procedural: `color` (per-vertex color)
    //   - asset: `tiles`, one atlas tile per face (-1 = untextured)
    // They can be combined: with a texture, the color multiplies it (white = texture as-is).
    struct BlockDesc {
        std::string name;
        bool solid = true;  // Collides
        bool opaque = true; // Blocks visibility (hides neighbouring faces)
        BlockShape shape = BlockShape::Cube;
        glm::vec4 color = glm::vec4(1.0f);
        std::array<int, kBlockFaceCount> tiles = {-1, -1, -1, -1, -1, -1};
        // Render pass; if unset: Opaque when opaque, otherwise Translucent
        std::optional<RenderLayer> layer;

        BlockDesc& setTile(int tile);                        // All faces
        BlockDesc& setTiles(int top, int bottom, int sides); // Top / bottom / sides
        bool textured() const;
        int tile(BlockFace face) const { return tiles[static_cast<int>(face)]; }
        RenderLayer renderLayer() const {
            return layer ? *layer : (opaque ? RenderLayer::Opaque : RenderLayer::Translucent);
        }
    };

    // Game block catalog. ID 0 (air: neither solid nor opaque, no shape) is pre-registered.
    class BlockRegistry {
    public:
        BlockRegistry();

        BlockId add(BlockDesc desc); // Returns the new (consecutive) ID
        // Unknown ID → assert in debug; returns air in release
        const BlockDesc& get(BlockId id) const;
        std::optional<BlockId> find(const std::string& name) const;
        std::size_t size() const { return m_blocks.size(); }

        bool isSolid(BlockId id) const { return get(id).solid; }
        bool isOpaque(BlockId id) const { return get(id).opaque; }

    private:
        std::vector<BlockDesc> m_blocks;
    };

    // UV rectangle of a tile in a columns × rows atlas (tile 0 at top-left, V pointing up)
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
