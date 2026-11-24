#pragma once

#include <mge/procgen/MeshData.h>

#include <glm/glm.hpp>

namespace mge::primitives {

    // Basic procedurally generated shapes. All centered at the origin, with normals, UVs, and color,
    // and triangles wound counter-clockwise viewed from outside. Closed shapes are watertight
    // (each edge is shared by exactly 2 triangles after welding duplicate seam vertices).

    // Plane on XZ (y = 0) facing +Y, subdivided into segX × segZ cells
    MeshData plane(float width, float depth, int segX = 1, int segZ = 1,
                   const glm::vec4& color = glm::vec4(1.0f));

    // Box with flat faces (24 vertices, 12 triangles)
    MeshData box(const glm::vec3& size, const glm::vec4& color = glm::vec4(1.0f));

    // Latitude/longitude sphere: segments around (>= 3), rings from pole to pole (>= 2)
    MeshData uvSphere(float radius, int segments = 32, int rings = 16,
                      const glm::vec4& color = glm::vec4(1.0f));

    // Geodesic sphere: subdivided icosahedron (shared vertices, no UV seam)
    MeshData icosphere(float radius, int subdivisions = 2, const glm::vec4& color = glm::vec4(1.0f));

    // Vertical cylinder of height `height` (y in [-h/2, h/2]), with optional caps
    MeshData cylinder(float radius, float height, int segments = 32, bool caps = true,
                      const glm::vec4& color = glm::vec4(1.0f));

    // Cone: base at y = -h/2, apex at y = +h/2, with closed base
    MeshData cone(float radius, float height, int segments = 32, const glm::vec4& color = glm::vec4(1.0f));

    // Torus in the XZ plane: majorRadius to tube center, minorRadius of the tube
    MeshData torus(float majorRadius, float minorRadius, int segments = 48, int sides = 24,
                   const glm::vec4& color = glm::vec4(1.0f));

    // Vertical capsule: cylinder of height `height` between two hemispheres of radius `radius`
    MeshData capsule(float radius, float height, int segments = 32, int rings = 8,
                     const glm::vec4& color = glm::vec4(1.0f));
}
