#include <mge/voxel/ChunkSnapshot.h>

#include <mge/voxel/VoxelWorld.h>

namespace mge {

    ChunkSnapshot ChunkSnapshot::capture(const VoxelWorld& world, const ChunkCoord& coord) {
        ChunkSnapshot snapshot;
        if (!world.isLoaded(coord)) return snapshot;
        snapshot.m_coord = coord;
        snapshot.m_blocks.assign(static_cast<std::size_t>(kPaddedSize) * kPaddedSize * kPaddedSize, kAir);

        // Los 27 chunks del cubo 3×3×3 (una búsqueda en el mapa cada uno, no una por bloque)
        const ChunkData* chunks[3][3][3];
        for (int dy = -1; dy <= 1; dy++)
            for (int dz = -1; dz <= 1; dz++)
                for (int dx = -1; dx <= 1; dx++)
                    chunks[dy + 1][dz + 1][dx + 1] = world.chunk({coord.x + dx, coord.y + dy, coord.z + dz});

        // Cada eje local de -1 a 16 cae en el chunk de la izquierda (-1), el propio (0..15) o el de la
        // derecha (16)
        auto split = [](int local, int& chunkOffset, int& inner) {
            chunkOffset = local < 0 ? 0 : (local >= kChunkSize ? 2 : 1);
            inner = local < 0 ? kChunkSize - 1 : (local >= kChunkSize ? 0 : local);
        };
        for (int y = -1; y <= kChunkSize; y++) {
            int cy, iy;
            split(y, cy, iy);
            for (int z = -1; z <= kChunkSize; z++) {
                int cz, iz;
                split(z, cz, iz);
                for (int x = -1; x <= kChunkSize; x++) {
                    int cx, ix;
                    split(x, cx, ix);
                    const ChunkData* source = chunks[cy][cz][cx];
                    if (source) snapshot.m_blocks[index(x, y, z)] = source->get(ix, iy, iz);
                }
            }
        }
        return snapshot;
    }
}
