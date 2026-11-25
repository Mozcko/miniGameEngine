#include <mge/voxel/ChunkMesher.h>

#include <mge/voxel/detail/MesherCommon.h>

#include <algorithm>
#include <array>
#include <utility>

namespace mge {

    namespace {
        using namespace detail;

        class Mesher {
        public:
            Mesher(const ChunkSnapshot& snapshot, const BlockRegistry& registry,
                   const ChunkMeshOptions& options)
                : m_registry(registry), m_snapshot(snapshot), m_options(options) {}

            ChunkMesh build() {
                for (int f = 0; f < kBlockFaceCount; f++) buildFace(f);
                return std::move(m_result);
            }

        private:
            // Local coordinates from -1 to 16: the snapshot already includes the neighbour border
            BlockId blockAt(const glm::ivec3& local) const { return m_snapshot.get(local); }

            bool occludes(const glm::ivec3& local) const {
                const BlockDesc& d = m_registry.get(blockAt(local));
                return d.opaque && d.shape == BlockShape::Cube;
            }

            MeshData& meshFor(const BlockDesc& desc) {
                switch (desc.renderLayer()) {
                case RenderLayer::Opaque: return m_result.opaque;
                case RenderLayer::Cutout: return m_result.cutout;
                case RenderLayer::Translucent: break;
                }
                return m_result.transparent;
            }

            std::vector<glm::vec3>* centroidsFor(const BlockDesc& desc) {
                return desc.renderLayer() == RenderLayer::Translucent ? &m_result.transparentFaceCentroids
                                                                      : nullptr;
            }

            static bool uniform(const AoValues& ao) {
                return ao[0] == ao[1] && ao[1] == ao[2] && ao[2] == ao[3];
            }

            void buildFace(int faceIndex) {
                const FaceDef& face = kFaces[faceIndex];
                const int u = (face.axis + 1) % 3;
                const int v = (face.axis + 2) % 3;
                std::array<MaskCell, kChunkSize * kChunkSize> mask;

                for (int slice = 0; slice < kChunkSize; slice++) {
                    mask.fill(MaskCell{});
                    // 1. Visible faces in this slice: mergeable ones go into the mask, the rest are emitted
                    // immediately
                    for (int j = 0; j < kChunkSize; j++) {
                        for (int i = 0; i < kChunkSize; i++) {
                            glm::ivec3 pos(0);
                            pos[face.axis] = slice;
                            pos[u] = i;
                            pos[v] = j;
                            const BlockId id = m_snapshot.get(pos);
                            if (id == kAir) continue;
                            const BlockDesc& desc = m_registry.get(id);
                            if (desc.shape == BlockShape::Cross) {
                                if (faceIndex == 0) emitCross(desc, pos);
                                continue;
                            }
                            if (desc.shape != BlockShape::Cube) continue;
                            if (!isFaceVisible(id, blockAt(pos + face.offset), m_registry)) continue;

                            const AoValues ao =
                                computeAo(pos, face, m_options.ambientOcclusion,
                                          [this](const glm::ivec3& p) { return occludes(p); });
                            if (m_options.greedy && !desc.textured() && uniform(ao))
                                mask[j * kChunkSize + i] = {id, ao[0]};
                            else
                                emitQuad(meshFor(desc), desc, face, faceIndex, glm::vec3(pos),
                                         glm::vec3(1.0f), ao, m_options, centroidsFor(desc));
                        }
                    }
                    // 2. Greedy: maximal rectangles of equal cells (widen along u, then grow along v)
                    solveGreedyMask(mask.data(), kChunkSize, kChunkSize,
                                    [&](const MaskCell& cell, int i, int j, int width, int height) {
                                        glm::ivec3 pos(0);
                                        pos[face.axis] = slice;
                                        pos[u] = i;
                                        pos[v] = j;
                                        glm::vec3 size(1.0f);
                                        size[u] = static_cast<float>(width);
                                        size[v] = static_cast<float>(height);
                                        const BlockDesc& desc = m_registry.get(cell.id);
                                        emitQuad(meshFor(desc), desc, face, faceIndex, glm::vec3(pos), size,
                                                 AoValues{cell.ao, cell.ao, cell.ao, cell.ao}, m_options,
                                                 centroidsFor(desc));
                                    });
                }
            }

            // Two diagonal planes, each double-sided (visible from both sides)
            void emitCross(const BlockDesc& desc, const glm::ivec3& pos) {
                MeshData& mesh = meshFor(desc);
                std::vector<glm::vec3>* centroids = centroidsFor(desc);
                UvRect uv{glm::vec2(0.0f), glm::vec2(0.0f)};
                if (desc.tile(BlockFace::Front) >= 0)
                    uv = m_options.atlas.tileUv(desc.tile(BlockFace::Front));
                const glm::vec2 uvs[4] = {uv.min, {uv.max.x, uv.min.y}, uv.max, {uv.min.x, uv.max.y}};
                const float s = 0.70710678f;
                const glm::vec3 planes[4][4] = {
                    {{0, 0, 0}, {1, 0, 1}, {1, 1, 1}, {0, 1, 0}},
                    {{1, 0, 1}, {0, 0, 0}, {0, 1, 0}, {1, 1, 1}},
                    {{0, 0, 1}, {1, 0, 0}, {1, 1, 0}, {0, 1, 1}},
                    {{1, 0, 0}, {0, 0, 1}, {0, 1, 1}, {1, 1, 0}},
                };
                const glm::vec3 normals[4] = {{-s, 0, s}, {s, 0, -s}, {s, 0, s}, {-s, 0, -s}};
                for (int p = 0; p < 4; p++) {
                    std::uint32_t idx[4];
                    glm::vec3 centroid(0.0f);
                    for (int i = 0; i < 4; i++) {
                        const glm::vec3 vertexPos = glm::vec3(pos) + planes[p][i];
                        centroid += vertexPos;
                        idx[i] = mesh.addVertex(vertexPos, normals[p], baseColor(desc, m_options), uvs[i]);
                    }
                    mesh.addQuad(idx[0], idx[1], idx[2], idx[3]);
                    if (centroids) centroids->push_back(centroid * 0.25f);
                }
            }

            const BlockRegistry& m_registry;
            const ChunkSnapshot& m_snapshot;
            const ChunkMeshOptions& m_options;
            ChunkMesh m_result;
        };
    }

    std::vector<std::uint32_t> sortQuadsBackToFront(const std::vector<std::uint32_t>& indices,
                                                    const std::vector<glm::vec3>& centroids,
                                                    const glm::vec3& cameraLocal) {
        const std::size_t quadCount = centroids.size();
        if (quadCount <= 1 || indices.size() != quadCount * 6) return indices;

        std::vector<std::pair<float, std::size_t>> order;
        order.reserve(quadCount);
        for (std::size_t q = 0; q < quadCount; ++q) {
            const glm::vec3 delta = centroids[q] - cameraLocal;
            order.emplace_back(glm::dot(delta, delta), q);
        }
        std::stable_sort(order.begin(), order.end(),
                         [](const auto& a, const auto& b) { return a.first > b.first; });

        std::vector<std::uint32_t> sorted(indices.size());
        for (std::size_t k = 0; k < quadCount; ++k) {
            const std::size_t src = order[k].second * 6;
            const std::size_t dst = k * 6;
            for (std::size_t i = 0; i < 6; ++i) sorted[dst + i] = indices[src + i];
        }
        return sorted;
    }

    ChunkMesh buildChunkMesh(const ChunkSnapshot& snapshot, const BlockRegistry& registry,
                             const ChunkMeshOptions& options) {
        if (!snapshot.valid()) return {};
        return Mesher(snapshot, registry, options).build();
    }

    ChunkMesh buildChunkMesh(const VoxelWorld& world, const ChunkCoord& coord,
                             const ChunkMeshOptions& options) {
        return buildChunkMesh(ChunkSnapshot::capture(world, coord), world.registry(), options);
    }
}
