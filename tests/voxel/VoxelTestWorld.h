#pragma once

#include <mge/voxel/VoxelWorld.h>

// Registry y generadores mínimos para los tests de voxel
namespace mge_test {

    struct TestBlocks {
        mge::BlockRegistry registry;
        mge::BlockId stone = 0, glass = 0, water = 0, flower = 0;

        TestBlocks() {
            mge::BlockDesc desc;
            desc.name = "stone";
            desc.color = {0.5f, 0.5f, 0.5f, 1.0f};
            stone = registry.add(desc);

            desc = {};
            desc.name = "glass";
            desc.opaque = false;
            glass = registry.add(desc);

            desc = {};
            desc.name = "water";
            desc.solid = false;
            desc.opaque = false;
            water = registry.add(desc);

            desc = {};
            desc.name = "flower";
            desc.solid = false;
            desc.opaque = false;
            desc.shape = mge::BlockShape::Cross;
            flower = registry.add(desc);
        }
    };

    // Suelo plano: todo lo que está debajo de y = height es `block`
    class FlatGenerator : public mge::TerrainGenerator {
    public:
        FlatGenerator(mge::BlockId block, int height) : m_block(block), m_height(height) {}
        void generate(const mge::ChunkCoord& coord, mge::ChunkData& chunk) const override {
            for (int y = 0; y < mge::kChunkSize; y++) {
                if (coord.y * mge::kChunkSize + y >= m_height) break;
                for (int z = 0; z < mge::kChunkSize; z++)
                    for (int x = 0; x < mge::kChunkSize; x++) chunk.set(x, y, z, m_block);
            }
        }

    private:
        mge::BlockId m_block;
        int m_height;
    };
}
