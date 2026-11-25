#pragma once

#include <mge/core/math/Geometry.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace mge {

    // CPU mesh, independent of the GPU (generated and tested without a window).
    // Separate arrays (SoA) of equal length: every vertex has position, normal, color, and UV.
    // indices: triangles (3 per triangle), counter-clockwise viewed from outside.
    struct MeshData {
        std::vector<glm::vec3> positions;
        std::vector<glm::vec3> normals;
        std::vector<glm::vec4> colors;
        std::vector<glm::vec2> uvs;
        std::vector<std::uint32_t> indices;

        std::uint32_t addVertex(const glm::vec3& position, const glm::vec3& normal = glm::vec3(0.0f),
                                const glm::vec4& color = glm::vec4(1.0f),
                                const glm::vec2& uv = glm::vec2(0.0f));
        void addTriangle(std::uint32_t a, std::uint32_t b, std::uint32_t c);
        // Two triangles: (a, b, c) and (a, c, d)
        void addQuad(std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t d);

        std::size_t vertexCount() const { return positions.size(); }
        std::size_t triangleCount() const { return indices.size() / 3; }
        bool empty() const { return positions.empty(); }

        void reserve(std::size_t vertices, std::size_t triangles);
        void clear();

        Aabb bounds() const;
        // Empty if the mesh is valid; otherwise describes the first problem found
        std::string validate() const;
    };

    // Exports to Wavefront OBJ (positions + per-vertex color, UVs, normals) to inspect
    // the mesh in Blender or any viewer. Returns false if the file could not be written.
    bool writeObj(const MeshData& mesh, const std::filesystem::path& path);
}
