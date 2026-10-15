#include <mge/procgen/detail/MeshBuilding.h>

#include <glm/gtc/constants.hpp>

#include <cmath>

namespace mge::detail {

    void addOrientedTriangle(MeshData& mesh, std::uint32_t a, std::uint32_t b, std::uint32_t c) {
        const glm::vec3 faceNormal =
            glm::cross(mesh.positions[b] - mesh.positions[a], mesh.positions[c] - mesh.positions[a]);
        const glm::vec3 vertexNormals = mesh.normals[a] + mesh.normals[b] + mesh.normals[c];
        if (glm::dot(faceNormal, vertexNormals) < 0.0f)
            mesh.addTriangle(a, c, b);
        else
            mesh.addTriangle(a, b, c);
    }

    void addOrientedQuad(MeshData& mesh, std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t d) {
        addOrientedTriangle(mesh, a, b, c);
        addOrientedTriangle(mesh, a, c, d);
    }

    void revolve(MeshData& mesh, const std::vector<Row>& rows, int segments, const glm::vec4& color) {
        const std::uint32_t base = static_cast<std::uint32_t>(mesh.vertexCount());
        const std::uint32_t stride = static_cast<std::uint32_t>(segments + 1);
        for (const Row& row : rows) {
            for (int s = 0; s <= segments; s++) {
                const float u = static_cast<float>(s) / static_cast<float>(segments);
                const float angle = u * glm::two_pi<float>();
                const glm::vec3 radial(std::cos(angle), 0.0f, std::sin(angle));
                const glm::vec3 position = radial * row.radius + glm::vec3(0.0f, row.y, 0.0f);
                const glm::vec3 normal =
                    glm::normalize(radial * row.normal.x + glm::vec3(0.0f, row.normal.y, 0.0f));
                mesh.addVertex(position, normal, color, {u, row.v});
            }
        }
        for (std::size_t r = 0; r + 1 < rows.size(); r++) {
            const bool topPole = rows[r].radius <= 0.0f;
            const bool bottomPole = rows[r + 1].radius <= 0.0f;
            for (std::uint32_t s = 0; s < stride - 1; s++) {
                const std::uint32_t a = base + static_cast<std::uint32_t>(r) * stride + s;
                const std::uint32_t b = a + 1;
                const std::uint32_t c = a + stride + 1;
                const std::uint32_t d = a + stride;
                if (topPole && bottomPole) continue;
                if (topPole)
                    addOrientedTriangle(mesh, a, c, d);
                else if (bottomPole)
                    addOrientedTriangle(mesh, a, b, d);
                else
                    addOrientedQuad(mesh, a, b, c, d);
            }
        }
    }
}
