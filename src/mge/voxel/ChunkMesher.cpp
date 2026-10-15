#include <mge/voxel/ChunkMesher.h>

namespace mge {

    namespace {
        struct FaceDef {
            glm::ivec3 offset; // Dirección al vecino
            glm::vec3 normal;
            glm::vec3 corners[4]; // Cubo unitario, antihorario visto desde afuera
        };

        // Mismo orden que BlockFace. Esquinas: (uMin,vMin), (uMax,vMin), (uMax,vMax), (uMin,vMax)
        const FaceDef kFaces[kBlockFaceCount] = {
            {{0, 0, 1}, {0, 0, 1}, {{0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}}},   // Front +Z
            {{0, 0, -1}, {0, 0, -1}, {{1, 0, 0}, {0, 0, 0}, {0, 1, 0}, {1, 1, 0}}}, // Back -Z
            {{-1, 0, 0}, {-1, 0, 0}, {{0, 0, 0}, {0, 0, 1}, {0, 1, 1}, {0, 1, 0}}}, // Left -X
            {{1, 0, 0}, {1, 0, 0}, {{1, 0, 1}, {1, 0, 0}, {1, 1, 0}, {1, 1, 1}}},   // Right +X
            {{0, 1, 0}, {0, 1, 0}, {{0, 1, 1}, {1, 1, 1}, {1, 1, 0}, {0, 1, 0}}},   // Top +Y
            {{0, -1, 0}, {0, -1, 0}, {{0, 0, 0}, {1, 0, 0}, {1, 0, 1}, {0, 0, 1}}}, // Bottom -Y
        };

        UvRect uvFor(const BlockDesc& desc, BlockFace face, const TextureAtlas& atlas) {
            const int tile = desc.tile(face);
            if (tile < 0) return {glm::vec2(0.0f), glm::vec2(0.0f)};
            return atlas.tileUv(tile);
        }

        void addQuad(MeshData& mesh, const glm::vec3 (&corners)[4], const glm::vec3& base,
                     const glm::vec3& normal, const glm::vec4& color, const UvRect& uv) {
            const glm::vec2 uvs[4] = {uv.min, {uv.max.x, uv.min.y}, uv.max, {uv.min.x, uv.max.y}};
            std::uint32_t idx[4];
            for (int i = 0; i < 4; i++) idx[i] = mesh.addVertex(base + corners[i], normal, color, uvs[i]);
            mesh.addQuad(idx[0], idx[1], idx[2], idx[3]);
        }

        // Dos planos diagonales, cada uno con sus dos caras (se ven desde ambos lados)
        void addCross(MeshData& mesh, const glm::vec3& base, const glm::vec4& color, const UvRect& uv) {
            const float s = 0.70710678f;
            const glm::vec3 diag1[4] = {{0, 0, 0}, {1, 0, 1}, {1, 1, 1}, {0, 1, 0}};
            const glm::vec3 diag1Back[4] = {{1, 0, 1}, {0, 0, 0}, {0, 1, 0}, {1, 1, 1}};
            const glm::vec3 diag2[4] = {{0, 0, 1}, {1, 0, 0}, {1, 1, 0}, {0, 1, 1}};
            const glm::vec3 diag2Back[4] = {{1, 0, 0}, {0, 0, 1}, {0, 1, 1}, {1, 1, 0}};
            addQuad(mesh, diag1, base, {-s, 0, s}, color, uv);
            addQuad(mesh, diag1Back, base, {s, 0, -s}, color, uv);
            addQuad(mesh, diag2, base, {s, 0, s}, color, uv);
            addQuad(mesh, diag2Back, base, {-s, 0, -s}, color, uv);
        }

        // ¿Se ve la cara de `self` que mira a `neighbor`?
        bool faceVisible(const BlockRegistry& registry, BlockId self, BlockId neighbor) {
            const BlockDesc& n = registry.get(neighbor);
            if (n.opaque && n.shape == BlockShape::Cube) return false; // Tapada
            return registry.get(self).opaque || self != neighbor;      // Agua contra agua: oculta
        }
    }

    ChunkMesh buildChunkMesh(const VoxelWorld& world, const ChunkCoord& coord,
                             const ChunkMeshOptions& options) {
        ChunkMesh result;
        const ChunkData* data = world.chunk(coord);
        if (!data) return result;

        const BlockRegistry& registry = world.registry();
        const glm::ivec3 origin = chunkOrigin(coord);

        // Bloque vecino: dentro del chunk se lee directo, afuera se pregunta al mundo
        auto blockAtLocal = [&](const glm::ivec3& p) {
            return ChunkData::inside(p.x, p.y, p.z) ? data->get(p) : world.getBlock(origin + p);
        };

        for (int y = 0; y < kChunkSize; y++) {
            for (int z = 0; z < kChunkSize; z++) {
                for (int x = 0; x < kChunkSize; x++) {
                    const BlockId id = data->get(x, y, z);
                    if (id == kAir) continue;
                    const BlockDesc& desc = registry.get(id);
                    if (desc.shape == BlockShape::None) continue;

                    MeshData& mesh = desc.opaque ? result.opaque : result.transparent;
                    const glm::ivec3 pos(x, y, z);
                    const glm::vec3 base(pos);

                    if (desc.shape == BlockShape::Cross) {
                        addCross(mesh, base, desc.color, uvFor(desc, BlockFace::Front, options.atlas));
                        continue;
                    }

                    for (int f = 0; f < kBlockFaceCount; f++) {
                        const FaceDef& face = kFaces[f];
                        if (!faceVisible(registry, id, blockAtLocal(pos + face.offset))) continue;
                        addQuad(mesh, face.corners, base, face.normal, desc.color,
                                uvFor(desc, static_cast<BlockFace>(f), options.atlas));
                    }
                }
            }
        }
        return result;
    }
}
