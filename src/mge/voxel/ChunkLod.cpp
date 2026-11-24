#include <mge/voxel/ChunkLod.h>

#include <mge/voxel/detail/MesherCommon.h>

#include <algorithm>
#include <array>
#include <vector>

namespace mge {

    namespace {
        using namespace detail;

        struct TopOpaqueSample {
            BlockId id = kAir;
            int topY = 0; // y + 1 of the highest opaque block in the cell (in chunk-local space)
        };

        // Scans a cell from top to bottom (along Y) looking for the first non-air opaque block and its height.
        inline TopOpaqueSample pickTopOpaqueSample(const ChunkSnapshot& snapshot,
                                                   const BlockRegistry* registry, int startX, int endX,
                                                   int startY, int endY, int startZ, int endZ) {
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
                        if (isOpaque(id)) return {id, y + 1};
                    }
                }
            }
            return {kAir, startY};
        }

        // 3D grid of super-blocks including a 1-super-block border in every direction.
        // Local coordinates in [-1, dim] for x, y, z. Also stores topY per cell to emit
        // top faces at the actual height of the highest opaque block (without an f-1 block step).
        class PaddedLodGrid {
        public:
            PaddedLodGrid(const ChunkSnapshot& snapshot, const BlockRegistry& registry, int factor, int dim)
                : m_factor(factor), m_dim(dim), m_paddedDim(dim + 2) {
                const std::size_t total = static_cast<std::size_t>(m_paddedDim * m_paddedDim * m_paddedDim);
                m_blocks.assign(total, kAir);
                m_topY.assign(total, 0);
                m_highestSy.assign(static_cast<std::size_t>(m_paddedDim * m_paddedDim), -2);

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

                            const TopOpaqueSample sample = pickTopOpaqueSample(
                                snapshot, &registry, startX, endX, startY, endY, startZ, endZ);
                            set(sx, sy, sz, sample.id, sample.topY);
                            if (sample.id != kAir) {
                                const std::size_t colIdx =
                                    static_cast<std::size_t>((sz + 1) * m_paddedDim + (sx + 1));
                                m_highestSy[colIdx] = sy;
                            }
                        }
                    }
                }
            }

            BlockId get(int sx, int sy, int sz) const {
                if (sx < -1 || sx > m_dim || sy < -1 || sy > m_dim || sz < -1 || sz > m_dim) {
                    return kAir;
                }
                return m_blocks[index(sx, sy, sz)];
            }

            BlockId get(const glm::ivec3& p) const { return get(p.x, p.y, p.z); }

            // Effective top height of a solid cell: only the highest opaque cell in the column
            // (exposed to the sky, with no opaque blocks above in the chunk) is trimmed to its real topY;
            // cells below (including caves) keep their full height (sy + 1) * factor.
            int cellTop(int sx, int sy, int sz) const {
                if (sx < -1 || sx > m_dim || sy < -1 || sy > m_dim || sz < -1 || sz > m_dim) {
                    return sy * m_factor;
                }
                const std::size_t idx = index(sx, sy, sz);
                if (m_blocks[idx] == kAir) return sy * m_factor;
                const std::size_t colIdx = static_cast<std::size_t>((sz + 1) * m_paddedDim + (sx + 1));
                if (sy != m_highestSy[colIdx]) {
                    return (sy + 1) * m_factor;
                }
                return m_topY[idx];
            }

        private:
            std::size_t index(int sx, int sy, int sz) const {
                return static_cast<std::size_t>(((sy + 1) * m_paddedDim + (sz + 1)) * m_paddedDim + (sx + 1));
            }

            void set(int sx, int sy, int sz, BlockId id, int topY) {
                const std::size_t idx = index(sx, sy, sz);
                m_blocks[idx] = id;
                m_topY[idx] = topY;
            }

            int m_factor;
            int m_dim;
            int m_paddedDim;
            std::vector<BlockId> m_blocks;
            std::vector<int> m_topY;
            std::vector<int> m_highestSy;
        };

        // Greedy mask cell for LOD: besides block and AO, tracks y0/y1 height to merge
        // top faces with the same y_top and side faces with the same vertical range.
        struct LodMaskCell {
            BlockId id = kAir;
            int ao = 0;
            int y0 = 0; // Top: yTop; partial side: bottom; full side or Bottom: 0
            int y1 = 0; // Top: yTop; partial side: top (> 0); full side or Bottom: 0
            bool operator==(const LodMaskCell& o) const {
                return id == o.id && ao == o.ao && y0 == o.y0 && y1 == o.y1;
            }
        };

        template <typename Callback>
        inline void solveLodGreedyMask(LodMaskCell* mask, int w, int h, Callback&& onRect) {
            for (int j = 0; j < h; ++j) {
                for (int i = 0; i < w;) {
                    const LodMaskCell cell = mask[j * w + i];
                    if (cell.id == kAir) {
                        ++i;
                        continue;
                    }
                    int width = 1;
                    while (i + width < w && mask[j * w + i + width] == cell) ++width;
                    int height = 1;
                    for (; j + height < h; ++height) {
                        bool rowMatches = true;
                        for (int k = 0; k < width && rowMatches; ++k) {
                            rowMatches = mask[(j + height) * w + i + k] == cell;
                        }
                        if (!rowMatches) break;
                    }
                    for (int dj = 0; dj < height; ++dj) {
                        for (int di = 0; di < width; ++di) {
                            mask[(j + dj) * w + i + di] = LodMaskCell{};
                        }
                    }
                    onRect(cell, i, j, width, height);
                    i += width;
                }
            }
        }

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
                        pickTopOpaqueSample(snapshot, &registry, startX, endX, startY, endY, startZ, endZ).id;
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
                        pickTopOpaqueSample(snapshot, nullptr, startX, endX, startY, endY, startZ, endZ).id;
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

        // 1. Top faces of transparent liquids (water) for LOD > 0.
        // Computed first to record the water height per column and ensure no opaque face
        // sits at the same height as or above the water quad (avoids depth-test failures
        // in shallow water without resorting to a depth epsilon).
        std::vector<int> columnWaterTopY(static_cast<std::size_t>(dim * dim), -1);
        const int topFaceIndex = static_cast<int>(BlockFace::Top);
        const FaceDef& topFace = kFaces[topFaceIndex];
        constexpr AoValues kLiquidAo = {3, 3, 3, 3};

        for (int sz = 0; sz < dim; ++sz) {
            const int startZ = sz * factor;
            const int endZ = startZ + factor;
            for (int sx = 0; sx < dim; ++sx) {
                const int startX = sx * factor;
                const int endX = startX + factor;
                for (int sy = 0; sy < dim; ++sy) {
                    const int startY = sy * factor;
                    const int endY = startY + factor;

                    // Find the highest non-air block inside the super-block
                    BlockId topBlockId = kAir;
                    int highestNonAirY = -1;
                    bool hasNonLiquidAtHighest = false;

                    for (int y = endY - 1; y >= startY; --y) {
                        for (int z = startZ; z < endZ; ++z) {
                            for (int x = startX; x < endX; ++x) {
                                const BlockId id = snapshot.get(x, y, z);
                                if (id == kAir) continue;
                                if (highestNonAirY == -1) {
                                    highestNonAirY = y;
                                    topBlockId = id;
                                }
                                if (y == highestNonAirY) {
                                    const BlockDesc& d = registry.get(id);
                                    if (d.renderLayer() != RenderLayer::Translucent || d.solid) {
                                        hasNonLiquidAtHighest = true;
                                    }
                                }
                            }
                        }
                        if (highestNonAirY != -1) break;
                    }

                    if (highestNonAirY == -1 || topBlockId == kAir || hasNonLiquidAtHighest) {
                        continue;
                    }

                    const BlockDesc& topDesc = registry.get(topBlockId);
                    if (topDesc.renderLayer() != RenderLayer::Translucent || topDesc.solid) {
                        continue;
                    }

                    // Verify that the cell above is air
                    int aboveStartY;
                    int aboveEndY;
                    if (sy + 1 < dim) {
                        aboveStartY = (sy + 1) * factor;
                        aboveEndY = aboveStartY + factor;
                    } else {
                        aboveStartY = kChunkSize;
                        aboveEndY = kChunkSize + 1;
                    }

                    bool cellAboveIsAir = true;
                    for (int y = aboveStartY; y < aboveEndY && cellAboveIsAir; ++y) {
                        for (int z = startZ; z < endZ && cellAboveIsAir; ++z) {
                            for (int x = startX; x < endX && cellAboveIsAir; ++x) {
                                if (snapshot.get(x, y, z) != kAir) {
                                    cellAboveIsAir = false;
                                }
                            }
                        }
                    }

                    if (!cellAboveIsAir) continue;

                    // Emit a flat f × f quad at the height of the highest liquid block
                    const int waterTopY = highestNonAirY + 1;
                    const std::size_t colIdx = static_cast<std::size_t>(sz * dim + sx);
                    columnWaterTopY[colIdx] = std::max(columnWaterTopY[colIdx], waterTopY);

                    const glm::vec3 worldPos(static_cast<float>(startX), static_cast<float>(waterTopY),
                                             static_cast<float>(startZ));
                    const glm::vec3 worldSize(static_cast<float>(factor), 0.0f, static_cast<float>(factor));

                    emitQuad(result.transparent, topDesc, topFace, topFaceIndex, worldPos, worldSize,
                             kLiquidAo, options, &result.transparentFaceCentroids);
                }
            }
        }

        auto occludes = [&](const glm::ivec3& p) {
            const BlockDesc& d = registry.get(grid.get(p));
            return d.opaque && d.shape == BlockShape::Cube;
        };

        std::vector<LodMaskCell> mask(static_cast<std::size_t>(dim * dim));

        for (int faceIndex = 0; faceIndex < kBlockFaceCount; ++faceIndex) {
            const FaceDef& face = kFaces[faceIndex];
            const int u = (face.axis + 1) % 3;
            const int v = (face.axis + 2) % 3;
            const bool isTopFace = (faceIndex == static_cast<int>(BlockFace::Top));
            const bool isBottomFace = (faceIndex == static_cast<int>(BlockFace::Bottom));

            for (int slice = 0; slice < dim; ++slice) {
                std::fill(mask.begin(), mask.end(), LodMaskCell{});

                // 1. Visible faces in this slice
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

                        const glm::ivec3 neighborPos = cellPos + face.offset;
                        const BlockId neighborId = grid.get(neighborPos);

                        int faceY0 = 0;
                        int faceY1 = 0;
                        if (isTopFace) {
                            if (!isFaceVisible(id, neighborId, registry)) continue;
                            faceY0 = grid.cellTop(cellPos.x, cellPos.y, cellPos.z);
                            // Safeguard: omit only the opaque face that matches the water quad height;
                            // faces above it, e.g. terrain over an underground lake, are kept.
                            const int waterTop =
                                columnWaterTopY[static_cast<std::size_t>(cellPos.z * dim + cellPos.x)];
                            if (waterTop >= 0 && faceY0 == waterTop) continue;
                            faceY1 = faceY0;
                        } else if (isBottomFace) {
                            if (!isFaceVisible(id, neighborId, registry)) continue;
                        } else {
                            // Side faces: reach up to cellTop and cover height steps between
                            // neighbouring columns
                            const int cellBottom = cellPos.y * factor;
                            const int cellTopY = grid.cellTop(cellPos.x, cellPos.y, cellPos.z);
                            const bool neighborOpaque = !isFaceVisible(id, neighborId, registry);
                            if (!neighborOpaque) {
                                faceY0 = cellBottom;
                                faceY1 = cellTopY;
                                // If the cell below (same block) had a partial step against the
                                // neighbour, absorb it here to emit a single vertical quad instead of two.
                                if (cellPos.y > 0) {
                                    const glm::ivec3 belowPos(cellPos.x, cellPos.y - 1, cellPos.z);
                                    const glm::ivec3 belowNeighbor = belowPos + face.offset;
                                    if (grid.get(belowPos) == id &&
                                        !isFaceVisible(id, grid.get(belowNeighbor), registry)) {
                                        const int belowNeighborTop =
                                            grid.cellTop(belowNeighbor.x, belowNeighbor.y, belowNeighbor.z);
                                        if (belowNeighborTop < cellBottom) {
                                            faceY0 = belowNeighborTop;
                                        }
                                    }
                                }
                            } else {
                                const int neighborTopY =
                                    grid.cellTop(neighborPos.x, neighborPos.y, neighborPos.z);
                                if (neighborTopY >= cellTopY) continue;
                                // If the cell above (same block) faces air, it will absorb this step
                                if (cellPos.y + 1 < dim && cellTopY == cellBottom + factor) {
                                    const glm::ivec3 abovePos(cellPos.x, cellPos.y + 1, cellPos.z);
                                    const glm::ivec3 aboveNeighbor = abovePos + face.offset;
                                    if (grid.get(abovePos) == id &&
                                        isFaceVisible(id, grid.get(aboveNeighbor), registry)) {
                                        continue;
                                    }
                                }
                                faceY0 = neighborTopY;
                                faceY1 = cellTopY;
                            }
                            if (faceY1 <= faceY0) continue;
                        }

                        const AoValues ao = computeAo(cellPos, face, options.ambientOcclusion, occludes);

                        if (options.greedy && !desc.textured() && uniform(ao)) {
                            if (isTopFace) {
                                mask[static_cast<std::size_t>(j * dim + i)] = {id, ao[0], faceY0, faceY0};
                            } else if (isBottomFace) {
                                mask[static_cast<std::size_t>(j * dim + i)] = {id, ao[0], 0, 0};
                            } else {
                                const int cellBottom = cellPos.y * factor;
                                const bool fullHeight =
                                    (faceY0 == cellBottom && faceY1 == cellBottom + factor);
                                if (fullHeight) {
                                    mask[static_cast<std::size_t>(j * dim + i)] = {id, ao[0], 0, 0};
                                } else {
                                    mask[static_cast<std::size_t>(j * dim + i)] = {id, ao[0], faceY0, faceY1};
                                }
                            }
                        } else {
                            glm::vec3 worldPos = glm::vec3(cellPos) * static_cast<float>(factor);
                            glm::vec3 worldSize(static_cast<float>(factor));
                            if (isTopFace) {
                                worldPos.y = static_cast<float>(faceY0);
                                worldSize.y = 0.0f;
                            } else if (!isBottomFace) {
                                worldPos.y = static_cast<float>(faceY0);
                                worldSize.y = static_cast<float>(faceY1 - faceY0);
                            }
                            emitQuad(result.opaque, desc, face, faceIndex, worldPos, worldSize, ao, options);

                            // Skirts on the 4 side borders exposed to air if there is no solid block below
                            if (options.lodSkirts && isSideBorder(faceIndex, cellPos, dim) &&
                                isFaceVisible(id, neighborId, registry)) {
                                if (cellPos.y == 0 || grid.get(cellPos.x, cellPos.y - 1, cellPos.z) == kAir) {
                                    emitSkirtQuad(result.opaque, desc, face, faceIndex, worldPos, worldSize,
                                                  static_cast<float>(factor), options);
                                }
                            }
                        }
                    }
                }

                // 2. Greedy meshing into maximal rectangles
                solveLodGreedyMask(
                    mask.data(), dim, dim, [&](const LodMaskCell& cell, int i, int j, int width, int height) {
                        glm::ivec3 cellPos(0);
                        cellPos[face.axis] = slice;
                        cellPos[u] = i;
                        cellPos[v] = j;

                        glm::vec3 worldSize(static_cast<float>(factor));
                        worldSize[u] = static_cast<float>(width * factor);
                        worldSize[v] = static_cast<float>(height * factor);
                        glm::vec3 worldPos = glm::vec3(cellPos) * static_cast<float>(factor);

                        if (isTopFace) {
                            worldPos.y = static_cast<float>(cell.y0);
                            worldSize.y = 0.0f;
                        } else if (!isBottomFace && cell.y1 > 0) {
                            worldPos.y = static_cast<float>(cell.y0);
                            worldSize.y = static_cast<float>(cell.y1 - cell.y0);
                        }

                        emitQuad(result.opaque, registry.get(cell.id), face, faceIndex, worldPos, worldSize,
                                 AoValues{cell.ao, cell.ao, cell.ao, cell.ao}, options);

                        // Skirts on the 4 side borders
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
