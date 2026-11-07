#include <mge/voxel/ChunkMesher.h>

#include <mge/voxel/detail/MesherCommon.h>

#include <array>

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
            // Local de -1 a 16: el snapshot ya trae el borde de los vecinos
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
                    // 1. Caras visibles de esta capa: las que se pueden unir van a la máscara, el resto sale
                    // ya
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
                                         glm::vec3(1.0f), ao, m_options);
                        }
                    }
                    // 2. Greedy: rectángulos máximos de celdas iguales (ancho en u, luego alto en v)
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
                                                 AoValues{cell.ao, cell.ao, cell.ao, cell.ao}, m_options);
                                    });
                }
            }

            // Dos planos diagonales, cada uno con sus dos caras (se ven desde ambos lados)
            void emitCross(const BlockDesc& desc, const glm::ivec3& pos) {
                MeshData& mesh = meshFor(desc);
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
                    for (int i = 0; i < 4; i++)
                        idx[i] = mesh.addVertex(glm::vec3(pos) + planes[p][i], normals[p],
                                                baseColor(desc, m_options), uvs[i]);
                    mesh.addQuad(idx[0], idx[1], idx[2], idx[3]);
                }
            }

            const BlockRegistry& m_registry;
            const ChunkSnapshot& m_snapshot;
            const ChunkMeshOptions& m_options;
            ChunkMesh m_result;
        };
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
