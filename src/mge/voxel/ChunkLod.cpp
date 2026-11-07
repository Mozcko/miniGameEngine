#include <mge/voxel/ChunkLod.h>

#include <mge/voxel/detail/MesherCommon.h>

#include <algorithm>
#include <array>
#include <vector>

namespace mge {

    namespace {
        using namespace detail;

        // Escanea una celda de arriba hacia abajo (en Y) buscando el primer bloque opaco no-aire.
        inline BlockId pickTopOpaque(const ChunkSnapshot& snapshot, const BlockRegistry* registry, int startX,
                                     int endX, int startY, int endY, int startZ, int endZ) {
            auto isOpaque = [&](BlockId id) {
                if (id == kAir) return false;
                if (!registry) return true;
                const BlockDesc& d = registry->get(id);
                return d.opaque && d.shape == BlockShape::Cube && d.renderLayer() == RenderLayer::Opaque;
            };

            for (int y = endY - 1; y >= startY; --y) {
                for (int z = startZ; z < endZ; ++z) {
                    for (int x = startX; x < endX; ++x) {
                        const BlockId id = snapshot.get(x, y, z);
                        if (isOpaque(id)) return id;
                    }
                }
            }
            return kAir;
        }

        // Grilla 3D de super-bloques incluyendo un borde de 1 super-bloque en cada dirección.
        // Coordenadas locales en [-1, dim] para x, y, z.
        class PaddedLodGrid {
        public:
            PaddedLodGrid(const ChunkSnapshot& snapshot, const BlockRegistry& registry, int factor, int dim)
                : m_dim(dim), m_paddedDim(dim + 2) {
                m_blocks.assign(static_cast<std::size_t>(m_paddedDim * m_paddedDim * m_paddedDim), kAir);

                for (int sy = -1; sy <= dim; ++sy) {
                    int startY, endY;
                    if (sy < 0) {
                        startY = -1;
                        endY = 0;
                    } else if (sy >= dim) {
                        startY = kChunkSize;
                        endY = kChunkSize + 1;
                    } else {
                        startY = sy * factor;
                        endY = startY + factor;
                    }

                    for (int sz = -1; sz <= dim; ++sz) {
                        int startZ, endZ;
                        if (sz < 0) {
                            startZ = -1;
                            endZ = 0;
                        } else if (sz >= dim) {
                            startZ = kChunkSize;
                            endZ = kChunkSize + 1;
                        } else {
                            startZ = sz * factor;
                            endZ = startZ + factor;
                        }

                        for (int sx = -1; sx <= dim; ++sx) {
                            int startX, endX;
                            if (sx < 0) {
                                startX = -1;
                                endX = 0;
                            } else if (sx >= dim) {
                                startX = kChunkSize;
                                endX = kChunkSize + 1;
                            } else {
                                startX = sx * factor;
                                endX = startX + factor;
                            }

                            const BlockId id =
                                pickTopOpaque(snapshot, &registry, startX, endX, startY, endY, startZ, endZ);
                            set(sx, sy, sz, id);
                        }
                    }
                }
            }

            BlockId get(int sx, int sy, int sz) const {
                if (sx < -1 || sx > m_dim || sy < -1 || sy > m_dim || sz < -1 || sz > m_dim) {
                    return kAir;
                }
                const std::size_t idx =
                    static_cast<std::size_t>(((sy + 1) * m_paddedDim + (sz + 1)) * m_paddedDim + (sx + 1));
                return m_blocks[idx];
            }

            BlockId get(const glm::ivec3& p) const { return get(p.x, p.y, p.z); }

        private:
            void set(int sx, int sy, int sz, BlockId id) {
                const std::size_t idx =
                    static_cast<std::size_t>(((sy + 1) * m_paddedDim + (sz + 1)) * m_paddedDim + (sx + 1));
                m_blocks[idx] = id;
            }

            int m_dim;
            int m_paddedDim;
            std::vector<BlockId> m_blocks;
        };

        inline bool isSideBorder(int faceIndex, const glm::ivec3& cellPos, int dim) {
            switch (static_cast<BlockFace>(faceIndex)) {
            case BlockFace::Front: return cellPos.z == dim - 1;
            case BlockFace::Back: return cellPos.z == 0;
            case BlockFace::Left: return cellPos.x == 0;
            case BlockFace::Right: return cellPos.x == dim - 1;
            default: return false;
            }
        }

        inline bool uniform(const AoValues& ao) {
            return ao[0] == ao[1] && ao[1] == ao[2] && ao[2] == ao[3];
        }

    } // namespace

    LodChunk downsample(const ChunkSnapshot& snapshot, const BlockRegistry& registry, int lod) {
        const int factor = std::min(16, 1 << std::max(0, lod));
        const int dim = kChunkSize / factor;
        LodChunk result(factor, dim);
        if (!snapshot.valid()) return result;

        for (int sy = 0; sy < dim; ++sy) {
            const int startY = sy * factor;
            const int endY = startY + factor;
            for (int sz = 0; sz < dim; ++sz) {
                const int startZ = sz * factor;
                const int endZ = startZ + factor;
                for (int sx = 0; sx < dim; ++sx) {
                    const int startX = sx * factor;
                    const int endX = startX + factor;
                    const BlockId id =
                        pickTopOpaque(snapshot, &registry, startX, endX, startY, endY, startZ, endZ);
                    result.set(sx, sy, sz, id);
                }
            }
        }
        return result;
    }

    LodChunk downsample(const ChunkSnapshot& snapshot, int lod) {
        const int factor = std::min(16, 1 << std::max(0, lod));
        const int dim = kChunkSize / factor;
        LodChunk result(factor, dim);
        if (!snapshot.valid()) return result;

        for (int sy = 0; sy < dim; ++sy) {
            const int startY = sy * factor;
            const int endY = startY + factor;
            for (int sz = 0; sz < dim; ++sz) {
                const int startZ = sz * factor;
                const int endZ = startZ + factor;
                for (int sx = 0; sx < dim; ++sx) {
                    const int startX = sx * factor;
                    const int endX = startX + factor;
                    const BlockId id =
                        pickTopOpaque(snapshot, nullptr, startX, endX, startY, endY, startZ, endZ);
                    result.set(sx, sy, sz, id);
                }
            }
        }
        return result;
    }

    ChunkMesh buildLodChunkMesh(const ChunkSnapshot& snapshot, const BlockRegistry& registry, int lod,
                                const ChunkMeshOptions& options) {
        if (!snapshot.valid()) return {};
        if (lod <= 0) {
            return buildChunkMesh(snapshot, registry, options);
        }

        const int factor = std::min(16, 1 << lod);
        const int dim = kChunkSize / factor;
        const PaddedLodGrid grid(snapshot, registry, factor, dim);

        ChunkMesh result;
        auto occludes = [&](const glm::ivec3& p) {
            const BlockDesc& d = registry.get(grid.get(p));
            return d.opaque && d.shape == BlockShape::Cube;
        };

        std::vector<MaskCell> mask(static_cast<std::size_t>(dim * dim));

        for (int faceIndex = 0; faceIndex < kBlockFaceCount; ++faceIndex) {
            const FaceDef& face = kFaces[faceIndex];
            const int u = (face.axis + 1) % 3;
            const int v = (face.axis + 2) % 3;

            for (int slice = 0; slice < dim; ++slice) {
                std::fill(mask.begin(), mask.end(), MaskCell{});

                // 1. Caras visibles de esta capa
                for (int j = 0; j < dim; ++j) {
                    for (int i = 0; i < dim; ++i) {
                        glm::ivec3 cellPos(0);
                        cellPos[face.axis] = slice;
                        cellPos[u] = i;
                        cellPos[v] = j;

                        const BlockId id = grid.get(cellPos);
                        if (id == kAir) continue;
                        const BlockDesc& desc = registry.get(id);
                        if (desc.shape != BlockShape::Cube || !desc.opaque) continue;

                        const BlockId neighborId = grid.get(cellPos + face.offset);
                        if (!isFaceVisible(id, neighborId, registry)) continue;

                        const AoValues ao = computeAo(cellPos, face, options.ambientOcclusion, occludes);

                        if (options.greedy && !desc.textured() && uniform(ao)) {
                            mask[static_cast<std::size_t>(j * dim + i)] = {id, ao[0]};
                        } else {
                            const glm::vec3 worldPos = glm::vec3(cellPos) * static_cast<float>(factor);
                            const glm::vec3 worldSize = glm::vec3(factor);
                            emitQuad(result.opaque, desc, face, faceIndex, worldPos, worldSize, ao, options);

                            // Faldas en los 4 bordes laterales si no hay bloque sólido debajo
                            if (options.lodSkirts && isSideBorder(faceIndex, cellPos, dim)) {
                                if (cellPos.y == 0 || grid.get(cellPos.x, cellPos.y - 1, cellPos.z) == kAir) {
                                    emitSkirtQuad(result.opaque, desc, face, faceIndex, worldPos, worldSize,
                                                  static_cast<float>(factor), options);
                                }
                            }
                        }
                    }
                }

                // 2. Greedy meshing en rectángulos máximos
                solveGreedyMask(
                    mask.data(), dim, dim, [&](const MaskCell& cell, int i, int j, int width, int height) {
                        glm::ivec3 cellPos(0);
                        cellPos[face.axis] = slice;
                        cellPos[u] = i;
                        cellPos[v] = j;

                        glm::vec3 worldSize(factor);
                        worldSize[u] = static_cast<float>(width * factor);
                        worldSize[v] = static_cast<float>(height * factor);
                        const glm::vec3 worldPos = glm::vec3(cellPos) * static_cast<float>(factor);

                        emitQuad(result.opaque, registry.get(cell.id), face, faceIndex, worldPos, worldSize,
                                 AoValues{cell.ao, cell.ao, cell.ao, cell.ao}, options);

                        // Faldas en los 4 bordes laterales
                        if (options.lodSkirts && isSideBorder(faceIndex, cellPos, dim)) {
                            if (cellPos.y == 0 || grid.get(cellPos.x, cellPos.y - 1, cellPos.z) == kAir) {
                                emitSkirtQuad(result.opaque, registry.get(cell.id), face, faceIndex, worldPos,
                                              worldSize, static_cast<float>(factor), options);
                            }
                        }
                    });
            }
        }

        return result;
    }

} // namespace mge
