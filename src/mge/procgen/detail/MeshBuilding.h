#pragma once
// Internal helpers for mge_procgen (not public API): shared by primitives and mesh operations.

#include <mge/procgen/MeshData.h>

#include <cstdint>
#include <vector>

namespace mge::detail {

    // Adds the triangle with the winding order whose geometric normal matches its vertex normals
    // (counter-clockwise viewed from the side the normals point to)
    void addOrientedTriangle(MeshData& mesh, std::uint32_t a, std::uint32_t b, std::uint32_t c);
    void addOrientedQuad(MeshData& mesh, std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t d);

    // One row of a surface of revolution around the Y axis
    struct Row {
        float radius;     // Distance to the axis (0 = pole)
        float y;          // Height
        glm::vec2 normal; // (radial component, Y component) of the normal
        float v;          // Vertical texture coordinate
    };

    // Revolves rows around the Y axis. Each row has segments + 1 vertices (the last duplicates
    // the first for the UV seam). Between two rows: quads, or a single triangle when one of them
    // is a pole (radius 0).
    void revolve(MeshData& mesh, const std::vector<Row>& rows, int segments, const glm::vec4& color);
}
