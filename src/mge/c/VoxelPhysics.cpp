// NOLINTBEGIN(readability-identifier-naming)
#include <mge/c/mge_c_internal.h>

namespace {

    class CTerrainGenerator : public mge::TerrainGenerator {
    public:
        CTerrainGenerator(mge_voxel_generator_fn fn, void* userData) : m_fn(fn), m_userData(userData) {}

        void generate(const mge::ChunkCoord& coord, mge::ChunkData& chunk) const override {
            if (!m_fn) return;
            const glm::ivec3 origin = mge::chunkOrigin(coord);
            for (int z = 0; z < static_cast<int>(mge::kChunkSize); ++z) {
                for (int y = 0; y < static_cast<int>(mge::kChunkSize); ++y) {
                    for (int x = 0; x < static_cast<int>(mge::kChunkSize); ++x) {
                        mge_block_id id = m_fn(origin.x + x, origin.y + y, origin.z + z, m_userData);
                        chunk.set(x, y, z, id);
                    }
                }
            }
        }

    private:
        mge_voxel_generator_fn m_fn = nullptr;
        void* m_userData = nullptr;
    };

} // namespace

extern "C" {

mge_block_registry* mge_block_registry_create(mge_app* app) {
    (void)app;
    return new mge_block_registry();
}

mge_block_id mge_block_register(mge_block_registry* reg, const mge_block_desc* desc) {
    if (!reg || !desc) return MGE_BLOCK_AIR;
    mge::BlockDesc d;
    d.name = desc->name ? desc->name : "block";
    d.solid = desc->is_solid;
    d.opaque = desc->is_opaque;
    d.color = glm::vec4(desc->color[0], desc->color[1], desc->color[2], desc->color[3]);
    return reg->registry.add(d);
}

void mge_block_registry_destroy(mge_block_registry* reg) {
    delete reg;
}

mge_voxel_world* mge_voxel_world_create(mge_app* app, mge_block_registry* reg,
                                        mge_voxel_generator_fn generator, void* user_data) {
    (void)app;
    auto* vw = new mge_voxel_world();
    if (reg) {
        vw->registry = &reg->registry;
    } else {
        vw->ownedRegistry = std::make_unique<mge::BlockRegistry>();
        vw->registry = vw->ownedRegistry.get();
    }

    std::unique_ptr<mge::TerrainGenerator> gen;
    if (generator) {
        gen = std::make_unique<CTerrainGenerator>(generator, user_data);
    }
    vw->world = std::make_unique<mge::VoxelWorld>(*vw->registry, std::move(gen));
    return vw;
}

void mge_voxel_world_destroy(mge_voxel_world* world) {
    delete world;
}

mge_block_id mge_voxel_world_get_block(mge_voxel_world* world, int32_t x, int32_t y, int32_t z) {
    if (!world || !world->world) return MGE_BLOCK_AIR;
    const glm::ivec3 pos{x, y, z};
    const mge::ChunkCoord coord = mge::chunkOf(pos);
    if (!world->world->isLoaded(coord)) {
        if (world->world->generator()) {
            world->world->loadChunk(coord);
        } else {
            return MGE_BLOCK_AIR;
        }
    }
    return world->world->getBlock(pos);
}

bool mge_voxel_world_set_block(mge_voxel_world* world, int32_t x, int32_t y, int32_t z, mge_block_id block) {
    if (!world || !world->world) return false;
    const glm::ivec3 pos{x, y, z};
    const mge::ChunkCoord coord = mge::chunkOf(pos);
    if (!world->world->isLoaded(coord)) {
        world->world->loadChunk(coord);
    }
    return world->world->setBlock(pos, block);
}

bool mge_voxel_world_raycast(mge_voxel_world* world, const float origin[3], const float dir[3],
                             float max_dist, mge_voxel_raycast_hit* out_hit) {
    if (!world || !world->world || !origin || !dir || !out_hit) return false;

    glm::vec3 rayOrig(origin[0], origin[1], origin[2]);
    glm::vec3 rayDir(dir[0], dir[1], dir[2]);
    auto hit = mge::raycastVoxels(*world->world, mge::Ray(rayOrig, rayDir), max_dist);
    if (!hit.has_value()) return false;

    out_hit->block[0] = hit->block.x;
    out_hit->block[1] = hit->block.y;
    out_hit->block[2] = hit->block.z;
    out_hit->normal[0] = hit->normal.x;
    out_hit->normal[1] = hit->normal.y;
    out_hit->normal[2] = hit->normal.z;
    out_hit->distance = hit->distance;
    out_hit->block_id = hit->blockId;
    out_hit->point[0] = hit->point.x;
    out_hit->point[1] = hit->point.y;
    out_hit->point[2] = hit->point.z;
    return true;
}

} // extern "C"
// NOLINTEND(readability-identifier-naming)
