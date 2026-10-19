#include <mge/voxel/ChunkMesher.h>

#include <array>

namespace mge {

    namespace {
        struct FaceDef {
            glm::ivec3 offset; // Dirección al vecino
            glm::vec3 normal;
            glm::vec3 corners[4]; // Cubo unitario, antihorario visto desde afuera
            int axis;             // Eje de la normal (0 x, 1 y, 2 z)
        };

        // Mismo orden que BlockFace. Esquinas: (uMin,vMin), (uMax,vMin), (uMax,vMax), (uMin,vMax)
        const FaceDef kFaces[kBlockFaceCount] = {
            {{0, 0, 1}, {0, 0, 1}, {{0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}}, 2},   // Front +Z
            {{0, 0, -1}, {0, 0, -1}, {{1, 0, 0}, {0, 0, 0}, {0, 1, 0}, {1, 1, 0}}, 2}, // Back -Z
            {{-1, 0, 0}, {-1, 0, 0}, {{0, 0, 0}, {0, 0, 1}, {0, 1, 1}, {0, 1, 0}}, 0}, // Left -X
            {{1, 0, 0}, {1, 0, 0}, {{1, 0, 1}, {1, 0, 0}, {1, 1, 0}, {1, 1, 1}}, 0},   // Right +X
            {{0, 1, 0}, {0, 1, 0}, {{0, 1, 1}, {1, 1, 1}, {1, 1, 0}, {0, 1, 0}}, 1},   // Top +Y
            {{0, -1, 0}, {0, -1, 0}, {{0, 0, 0}, {1, 0, 0}, {1, 0, 1}, {0, 0, 1}}, 1}, // Bottom -Y
        };

        using AoValues = std::array<int, 4>; // 0 (rincón cerrado) .. 3 (libre), por esquina

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
            // Celda pendiente de unir (greedy): bloque + oclusión uniforme (o -1 si no se puede unir)
            struct MaskCell {
                BlockId id = kAir;
                int ao = 0;
                bool operator==(const MaskCell& o) const { return id == o.id && ao == o.ao; }
            };

            // Local de -1 a 16: el snapshot ya trae el borde de los vecinos
            BlockId blockAt(const glm::ivec3& local) const { return m_snapshot.get(local); }

            bool occludes(const glm::ivec3& local) const {
                const BlockDesc& d = m_registry.get(blockAt(local));
                return d.opaque && d.shape == BlockShape::Cube;
            }

            // ¿Se ve la cara de `self` que mira a `neighbor`?
            bool faceVisible(BlockId self, BlockId neighbor) const {
                const BlockDesc& n = m_registry.get(neighbor);
                if (n.opaque && n.shape == BlockShape::Cube) return false; // Tapada
                return m_registry.get(self).opaque || self != neighbor;    // Agua contra agua: oculta
            }

            // Oclusión de cada esquina: mira los 3 bloques que rodean la esquina en la capa de enfrente
            AoValues ambientOcclusion(const glm::ivec3& pos, const FaceDef& face) const {
                AoValues ao = {3, 3, 3, 3};
                if (!m_options.ambientOcclusion) return ao;
                const glm::ivec3 front = pos + face.offset;
                const int u = (face.axis + 1) % 3;
                const int v = (face.axis + 2) % 3;
                for (int i = 0; i < 4; i++) {
                    glm::ivec3 du(0), dv(0);
                    du[u] = face.corners[i][u] > 0.5f ? 1 : -1;
                    dv[v] = face.corners[i][v] > 0.5f ? 1 : -1;
                    const int side1 = occludes(front + du);
                    const int side2 = occludes(front + dv);
                    const int corner = occludes(front + du + dv);
                    ao[i] = (side1 && side2) ? 0 : 3 - (side1 + side2 + corner);
                }
                return ao;
            }

            MeshData& meshFor(const BlockDesc& desc) {
                switch (desc.renderLayer()) {
                case RenderLayer::Opaque: return m_result.opaque;
                case RenderLayer::Cutout: return m_result.cutout;
                case RenderLayer::Translucent: break;
                }
                return m_result.transparent;
            }

            // Color de vértice base del bloque (ver ChunkMeshOptions::tintTextured)
            glm::vec4 baseColor(const BlockDesc& desc) const {
                return desc.textured() && !m_options.tintTextured ? glm::vec4(1.0f) : desc.color;
            }

            // Quad de la cara `face` del bloque en `pos`, estirado `size` bloques en los ejes de la cara
            void emitQuad(const BlockDesc& desc, const FaceDef& face, int faceIndex, const glm::ivec3& pos,
                          const glm::vec3& size, const AoValues& ao) {
                MeshData& mesh = meshFor(desc);
                UvRect uv{glm::vec2(0.0f), glm::vec2(0.0f)};
                const int tile = desc.tiles[faceIndex];
                if (tile >= 0) uv = m_options.atlas.tileUv(tile);
                const glm::vec2 uvs[4] = {uv.min, {uv.max.x, uv.min.y}, uv.max, {uv.min.x, uv.max.y}};

                std::uint32_t idx[4];
                for (int i = 0; i < 4; i++) {
                    const float shade = 1.0f - m_options.aoStrength * (3 - ao[i]) / 3.0f;
                    const glm::vec4 base = baseColor(desc);
                    const glm::vec4 color(glm::vec3(base) * shade, base.a);
                    idx[i] =
                        mesh.addVertex(glm::vec3(pos) + face.corners[i] * size, face.normal, color, uvs[i]);
                }
                // Diagonal hacia las esquinas más parecidas: evita que el AO se interpole en "X"
                // (anisotropía)
                if (ao[0] + ao[2] < ao[1] + ao[3])
                    mesh.addQuad(idx[1], idx[2], idx[3], idx[0]);
                else
                    mesh.addQuad(idx[0], idx[1], idx[2], idx[3]);
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
                            if (!faceVisible(id, blockAt(pos + face.offset))) continue;

                            const AoValues ao = ambientOcclusion(pos, face);
                            if (m_options.greedy && !desc.textured() && uniform(ao))
                                mask[j * kChunkSize + i] = {id, ao[0]};
                            else
                                emitQuad(desc, face, faceIndex, pos, glm::vec3(1.0f), ao);
                        }
                    }
                    // 2. Greedy: rectángulos máximos de celdas iguales (ancho en u, luego alto en v)
                    for (int j = 0; j < kChunkSize; j++) {
                        for (int i = 0; i < kChunkSize;) {
                            const MaskCell cell = mask[j * kChunkSize + i];
                            if (cell.id == kAir) {
                                i++;
                                continue;
                            }
                            int width = 1;
                            while (i + width < kChunkSize && mask[j * kChunkSize + i + width] == cell)
                                width++;
                            int height = 1;
                            for (; j + height < kChunkSize; height++) {
                                bool rowMatches = true;
                                for (int k = 0; k < width && rowMatches; k++)
                                    rowMatches = mask[(j + height) * kChunkSize + i + k] == cell;
                                if (!rowMatches) break;
                            }
                            for (int dj = 0; dj < height; dj++)
                                for (int di = 0; di < width; di++)
                                    mask[(j + dj) * kChunkSize + i + di] = MaskCell{};

                            glm::ivec3 pos(0);
                            pos[face.axis] = slice;
                            pos[u] = i;
                            pos[v] = j;
                            glm::vec3 size(1.0f);
                            size[u] = static_cast<float>(width);
                            size[v] = static_cast<float>(height);
                            emitQuad(m_registry.get(cell.id), face, faceIndex, pos, size,
                                     {cell.ao, cell.ao, cell.ao, cell.ao});
                            i += width;
                        }
                    }
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
                        idx[i] = mesh.addVertex(glm::vec3(pos) + planes[p][i], normals[p], baseColor(desc),
                                                uvs[i]);
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
