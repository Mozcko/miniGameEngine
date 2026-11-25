#pragma once

#include <mge/procgen/MeshData.h>

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

namespace mge {

    // --- Transform and combine ---

    // Applies the matrix to positions and normals (normals via inverse transpose, renormalized).
    // If the matrix mirrors (determinant < 0), flips triangle order so winding stays
    // counter-clockwise viewed from outside.
    void transformMesh(MeshData& mesh, const glm::mat4& matrix);

    // Appends src to the end of dst (src indices are offset)
    void appendMesh(MeshData& dst, const MeshData& src);
    MeshData mergeMeshes(const MeshData& a, const MeshData& b);

    // Merges identical vertices (position, normal, color, and UV within epsilon). Hard edges
    // (different normals) and UV seams are preserved.
    void weldVertices(MeshData& mesh, float epsilon = 1e-5f);

    // Reverses all triangles and flips normals (turns the mesh inside out)
    void flipWinding(MeshData& mesh);

    enum class NormalMode {
        Smooth, // Area-weighted average of faces sharing the same POSITION (smooths seams)
        Flat    // Each triangle gets its own 3 vertices with the face normal (faceted look)
    };
    void recomputeNormals(MeshData& mesh, NormalMode mode);

    // --- Build from 2D profiles ---

    // Triangulates a simple polygon (no self-intersections; concave allowed) via ear clipping.
    // Accepts clockwise or counter-clockwise input. Returns counter-clockwise triangle indices.
    std::vector<std::uint32_t> triangulatePolygon(const std::vector<glm::vec2>& polygon);

    // Extrudes an XY-plane polygon along Z, from -depth/2 to +depth/2, with end caps.
    // Flat side faces (hard edges). Watertight result.
    MeshData extrude(const std::vector<glm::vec2>& polygon, float depth,
                     const glm::vec4& color = glm::vec4(1.0f));

    // Lathe: revolves a profile (x = radius, y = height) around the Y axis.
    // Order the profile bottom-to-top for outward-facing normals.
    // A point with x = 0 is a pole (closes the shape).
    MeshData lathe(const std::vector<glm::vec2>& profile, int segments = 32,
                   const glm::vec4& color = glm::vec4(1.0f));

    // Sweep: carries a closed profile (XY plane) along a 3D path using parallel-transport frames
    // (no sudden twists). With caps = true closes the ends (when the path is not closed).
    MeshData sweep(const std::vector<glm::vec2>& profile, const std::vector<glm::vec3>& path,
                   bool closedPath = false, bool caps = true, const glm::vec4& color = glm::vec4(1.0f));
}
