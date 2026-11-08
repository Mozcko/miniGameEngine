#pragma once

#include <mge/procgen/MeshData.h>
#include <mge/voxel/Block.h>
#include <mge/voxel/ChunkMesher.h>

#include <array>
#include <cstdint>

namespace mge::detail {

    struct FaceDef {
        glm::ivec3 offset; // Dirección al vecino
        glm::vec3 normal;
        glm::vec3 corners[4]; // Cubo unitario, antihorario visto desde afuera
        int axis;             // Eje de la normal (0 x, 1 y, 2 z)
    };

    // Mismo orden que BlockFace. Esquinas: (uMin,vMin), (uMax,vMin), (uMax,vMax), (uMin,vMax)
    inline constexpr FaceDef kFaces[kBlockFaceCount] = {
        {{0, 0, 1}, {0, 0, 1}, {{0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}}, 2},   // Front +Z
        {{0, 0, -1}, {0, 0, -1}, {{1, 0, 0}, {0, 0, 0}, {0, 1, 0}, {1, 1, 0}}, 2}, // Back -Z
        {{-1, 0, 0}, {-1, 0, 0}, {{0, 0, 0}, {0, 0, 1}, {0, 1, 1}, {0, 1, 0}}, 0}, // Left -X
        {{1, 0, 0}, {1, 0, 0}, {{1, 0, 1}, {1, 0, 0}, {1, 1, 0}, {1, 1, 1}}, 0},   // Right +X
        {{0, 1, 0}, {0, 1, 0}, {{0, 1, 1}, {1, 1, 1}, {1, 1, 0}, {0, 1, 0}}, 1},   // Top +Y
        {{0, -1, 0}, {0, -1, 0}, {{0, 0, 0}, {1, 0, 0}, {1, 0, 1}, {0, 0, 1}}, 1}, // Bottom -Y
    };

    using AoValues = std::array<int, 4>; // 0 (rincón cerrado) .. 3 (libre), por esquina

    struct MaskCell {
        BlockId id = kAir;
        int ao = 0;
        bool operator==(const MaskCell& o) const { return id == o.id && ao == o.ao; }
    };

    inline bool isFaceVisible(BlockId self, BlockId neighbor, const BlockRegistry& registry) {
        const BlockDesc& n = registry.get(neighbor);
        if (n.opaque && n.shape == BlockShape::Cube) return false;
        return registry.get(self).opaque || self != neighbor;
    }

    template <typename OccludesFn>
    inline AoValues computeAo(const glm::ivec3& pos, const FaceDef& face, bool enableAo,
                              OccludesFn&& occludes) {
        AoValues ao = {3, 3, 3, 3};
        if (!enableAo) return ao;
        const glm::ivec3 front = pos + face.offset;
        const int u = (face.axis + 1) % 3;
        const int v = (face.axis + 2) % 3;
        for (int i = 0; i < 4; i++) {
            glm::ivec3 du(0), dv(0);
            du[u] = face.corners[i][u] > 0.5f ? 1 : -1;
            dv[v] = face.corners[i][v] > 0.5f ? 1 : -1;
            const int side1 = occludes(front + du) ? 1 : 0;
            const int side2 = occludes(front + dv) ? 1 : 0;
            const int corner = occludes(front + du + dv) ? 1 : 0;
            ao[i] = (side1 && side2) ? 0 : 3 - (side1 + side2 + corner);
        }
        return ao;
    }

    inline glm::vec4 baseColor(const BlockDesc& desc, const ChunkMeshOptions& options) {
        return desc.textured() && !options.tintTextured ? glm::vec4(1.0f) : desc.color;
    }

    inline void emitQuad(MeshData& mesh, const BlockDesc& desc, const FaceDef& face, int faceIndex,
                         const glm::vec3& worldPos, const glm::vec3& worldSize, const AoValues& ao,
                         const ChunkMeshOptions& options) {
        UvRect uv{glm::vec2(0.0f), glm::vec2(0.0f)};
        const int tile = desc.tiles[faceIndex];
        if (tile >= 0) uv = options.atlas.tileUv(tile);
        const glm::vec2 uvs[4] = {uv.min, {uv.max.x, uv.min.y}, uv.max, {uv.min.x, uv.max.y}};

        std::uint32_t idx[4];
        for (int i = 0; i < 4; i++) {
            const float shade = 1.0f - options.aoStrength * (3 - ao[i]) / 3.0f;
            const glm::vec4 base = baseColor(desc, options);
            const glm::vec4 color(glm::vec3(base) * shade, base.a);
            idx[i] = mesh.addVertex(worldPos + face.corners[i] * worldSize, face.normal, color, uvs[i]);
        }
        if (ao[0] + ao[2] < ao[1] + ao[3])
            mesh.addQuad(idx[1], idx[2], idx[3], idx[0]);
        else
            mesh.addQuad(idx[0], idx[1], idx[2], idx[3]);
    }

    inline void emitSkirtQuad(MeshData& mesh, const BlockDesc& desc, const FaceDef& face, int faceIndex,
                              const glm::vec3& worldPos, const glm::vec3& worldSize, float skirtDepth,
                              const ChunkMeshOptions& options) {
        UvRect uv{glm::vec2(0.0f), glm::vec2(0.0f)};
        const int tile = desc.tiles[faceIndex];
        if (tile >= 0) uv = options.atlas.tileUv(tile);
        const glm::vec2 uvs[4] = {uv.min, {uv.max.x, uv.min.y}, uv.max, {uv.min.x, uv.max.y}};

        std::uint32_t idx[4];
        for (int i = 0; i < 4; i++) {
            glm::vec3 c = worldPos + face.corners[i] * worldSize;
            if (face.corners[i].y > 0.5f) {
                c.y = worldPos.y;
            } else {
                c.y = worldPos.y - skirtDepth;
            }
            idx[i] = mesh.addVertex(c, face.normal, baseColor(desc, options), uvs[i]);
        }
        mesh.addQuad(idx[0], idx[1], idx[2], idx[3]);
    }

    template <typename Callback>
    inline void solveGreedyMask(MaskCell* mask, int w, int h, Callback&& onRect) {
        for (int j = 0; j < h; j++) {
            for (int i = 0; i < w;) {
                const MaskCell cell = mask[j * w + i];
                if (cell.id == kAir) {
                    i++;
                    continue;
                }
                int width = 1;
                while (i + width < w && mask[j * w + i + width] == cell) width++;
                int height = 1;
                for (; j + height < h; height++) {
                    bool rowMatches = true;
                    for (int k = 0; k < width && rowMatches; k++)
                        rowMatches = mask[(j + height) * w + i + k] == cell;
                    if (!rowMatches) break;
                }
                for (int dj = 0; dj < height; dj++)
                    for (int di = 0; di < width; di++) mask[(j + dj) * w + i + di] = MaskCell{};

                onRect(cell, i, j, width, height);
                i += width;
            }
        }
    }

} // namespace mge::detail
