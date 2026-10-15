#include <mge/procgen/Primitives.h>
#include <mge/procgen/detail/MeshBuilding.h>

#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cmath>
#include <map>
#include <utility>
#include <vector>

namespace mge::primitives {

    using detail::addOrientedQuad;
    using detail::addOrientedTriangle;
    using detail::revolve;
    using detail::Row;

    namespace {
        constexpr float kTwoPi = glm::two_pi<float>();
        constexpr float kPi = glm::pi<float>();

        // Tapa plana horizontal (disco) a la altura y, mirando hacia normalY (+1 o -1)
        void addCap(MeshData& mesh, float radius, float y, float normalY, int segments,
                    const glm::vec4& color) {
            const glm::vec3 normal(0.0f, normalY, 0.0f);
            const std::uint32_t center = mesh.addVertex({0.0f, y, 0.0f}, normal, color, {0.5f, 0.5f});
            const std::uint32_t first = static_cast<std::uint32_t>(mesh.vertexCount());
            for (int s = 0; s <= segments; s++) {
                const float angle = static_cast<float>(s) / static_cast<float>(segments) * kTwoPi;
                const glm::vec2 dir(std::cos(angle), std::sin(angle));
                mesh.addVertex({dir.x * radius, y, dir.y * radius}, normal, color, dir * 0.5f + 0.5f);
            }
            for (std::uint32_t s = 0; s < static_cast<std::uint32_t>(segments); s++) {
                addOrientedTriangle(mesh, center, first + s, first + s + 1);
            }
        }
    }

    MeshData plane(float width, float depth, int segX, int segZ, const glm::vec4& color) {
        segX = std::max(segX, 1);
        segZ = std::max(segZ, 1);
        MeshData mesh;
        mesh.reserve(static_cast<std::size_t>((segX + 1) * (segZ + 1)),
                     static_cast<std::size_t>(2 * segX * segZ));
        for (int j = 0; j <= segZ; j++) {
            for (int i = 0; i <= segX; i++) {
                const float u = static_cast<float>(i) / static_cast<float>(segX);
                const float v = static_cast<float>(j) / static_cast<float>(segZ);
                mesh.addVertex({(u - 0.5f) * width, 0.0f, (v - 0.5f) * depth}, {0.0f, 1.0f, 0.0f}, color,
                               {u, v});
            }
        }
        const std::uint32_t stride = static_cast<std::uint32_t>(segX + 1);
        for (std::uint32_t j = 0; j < static_cast<std::uint32_t>(segZ); j++) {
            for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(segX); i++) {
                const std::uint32_t a = j * stride + i;
                addOrientedQuad(mesh, a, a + 1, a + stride + 1, a + stride);
            }
        }
        return mesh;
    }

    MeshData box(const glm::vec3& size, const glm::vec4& color) {
        MeshData mesh;
        mesh.reserve(24, 12);
        const glm::vec3 h = size * 0.5f;
        // Cada cara: normal y dos ejes de la cara (u, v)
        const glm::vec3 faces[6][3] = {
            {{1, 0, 0}, {0, 0, -1}, {0, 1, 0}}, {{-1, 0, 0}, {0, 0, 1}, {0, 1, 0}},
            {{0, 1, 0}, {1, 0, 0}, {0, 0, -1}}, {{0, -1, 0}, {1, 0, 0}, {0, 0, 1}},
            {{0, 0, 1}, {1, 0, 0}, {0, 1, 0}},  {{0, 0, -1}, {-1, 0, 0}, {0, 1, 0}},
        };
        for (const auto& face : faces) {
            const glm::vec3 n = face[0], du = face[1], dv = face[2];
            const glm::vec3 center = n * h;
            const glm::vec3 eu = du * h, ev = dv * h;
            const std::uint32_t a = mesh.addVertex(center - eu - ev, n, color, {0, 0});
            const std::uint32_t b = mesh.addVertex(center + eu - ev, n, color, {1, 0});
            const std::uint32_t c = mesh.addVertex(center + eu + ev, n, color, {1, 1});
            const std::uint32_t d = mesh.addVertex(center - eu + ev, n, color, {0, 1});
            addOrientedQuad(mesh, a, b, c, d);
        }
        return mesh;
    }

    MeshData uvSphere(float radius, int segments, int rings, const glm::vec4& color) {
        segments = std::max(segments, 3);
        rings = std::max(rings, 2);
        std::vector<Row> rows;
        for (int r = 0; r <= rings; r++) {
            const float v = static_cast<float>(r) / static_cast<float>(rings);
            const float theta = v * kPi; // 0 = polo norte
            const float ringRadius = (r == 0 || r == rings) ? 0.0f : std::sin(theta) * radius;
            rows.push_back(
                {ringRadius, std::cos(theta) * radius, {std::sin(theta), std::cos(theta)}, 1.0f - v});
        }
        MeshData mesh;
        revolve(mesh, rows, segments, color);
        return mesh;
    }

    MeshData icosphere(float radius, int subdivisions, const glm::vec4& color) {
        const float t = (1.0f + std::sqrt(5.0f)) * 0.5f;
        std::vector<glm::vec3> points = {{-1, t, 0}, {1, t, 0}, {-1, -t, 0}, {1, -t, 0},
                                         {0, -1, t}, {0, 1, t}, {0, -1, -t}, {0, 1, -t},
                                         {t, 0, -1}, {t, 0, 1}, {-t, 0, -1}, {-t, 0, 1}};
        for (glm::vec3& p : points) p = glm::normalize(p);
        std::vector<glm::uvec3> faces = {{0, 11, 5}, {0, 5, 1},  {0, 1, 7},   {0, 7, 10}, {0, 10, 11},
                                         {1, 5, 9},  {5, 11, 4}, {11, 10, 2}, {10, 7, 6}, {7, 1, 8},
                                         {3, 9, 4},  {3, 4, 2},  {3, 2, 6},   {3, 6, 8},  {3, 8, 9},
                                         {4, 9, 5},  {2, 4, 11}, {6, 2, 10},  {8, 6, 7},  {9, 8, 1}};

        for (int level = 0; level < std::max(subdivisions, 0); level++) {
            std::map<std::pair<std::uint32_t, std::uint32_t>, std::uint32_t> midpoints;
            auto midpoint = [&](std::uint32_t a, std::uint32_t b) {
                auto key = std::minmax(a, b);
                auto it = midpoints.find(key);
                if (it != midpoints.end()) return it->second;
                points.push_back(glm::normalize((points[a] + points[b]) * 0.5f));
                const auto index = static_cast<std::uint32_t>(points.size() - 1);
                midpoints.emplace(key, index);
                return index;
            };
            std::vector<glm::uvec3> next;
            next.reserve(faces.size() * 4);
            for (const glm::uvec3& f : faces) {
                const std::uint32_t ab = midpoint(f.x, f.y), bc = midpoint(f.y, f.z), ca = midpoint(f.z, f.x);
                next.insert(next.end(), {{f.x, ab, ca}, {f.y, bc, ab}, {f.z, ca, bc}, {ab, bc, ca}});
            }
            faces = std::move(next);
        }

        MeshData mesh;
        mesh.reserve(points.size(), faces.size());
        for (const glm::vec3& p : points) {
            // UV esférico (con costura: suficiente para mallas de color/normal)
            const glm::vec2 uv(0.5f + std::atan2(p.z, p.x) / kTwoPi, 0.5f + std::asin(p.y) / kPi);
            mesh.addVertex(p * radius, p, color, uv);
        }
        for (const glm::uvec3& f : faces) addOrientedTriangle(mesh, f.x, f.y, f.z);
        return mesh;
    }

    MeshData cylinder(float radius, float height, int segments, bool caps, const glm::vec4& color) {
        segments = std::max(segments, 3);
        const float h = height * 0.5f;
        MeshData mesh;
        revolve(mesh, {{radius, h, {1.0f, 0.0f}, 1.0f}, {radius, -h, {1.0f, 0.0f}, 0.0f}}, segments, color);
        if (caps) {
            addCap(mesh, radius, h, 1.0f, segments, color);
            addCap(mesh, radius, -h, -1.0f, segments, color);
        }
        return mesh;
    }

    MeshData cone(float radius, float height, int segments, const glm::vec4& color) {
        segments = std::max(segments, 3);
        const float h = height * 0.5f;
        // Normal de la superficie lateral: perpendicular a la generatriz
        const glm::vec2 slope = glm::normalize(glm::vec2(height, radius));
        MeshData mesh;
        revolve(mesh, {{0.0f, h, slope, 1.0f}, {radius, -h, slope, 0.0f}}, segments, color);
        addCap(mesh, radius, -h, -1.0f, segments, color);
        return mesh;
    }

    MeshData torus(float majorRadius, float minorRadius, int segments, int sides, const glm::vec4& color) {
        segments = std::max(segments, 3);
        sides = std::max(sides, 3);
        // El toro es un círculo (el tubo) revolucionado alrededor del eje Y
        std::vector<Row> rows;
        for (int i = 0; i <= sides; i++) {
            const float v = static_cast<float>(i) / static_cast<float>(sides);
            const float phi = v * kTwoPi;
            const glm::vec2 dir(std::cos(phi), std::sin(phi));
            rows.push_back({majorRadius + minorRadius * dir.x, minorRadius * dir.y, dir, v});
        }
        MeshData mesh;
        revolve(mesh, rows, segments, color);
        return mesh;
    }

    MeshData capsule(float radius, float height, int segments, int rings, const glm::vec4& color) {
        segments = std::max(segments, 3);
        rings = std::max(rings, 1);
        const float h = height * 0.5f;
        const float totalHeight = height + 2.0f * radius;
        std::vector<Row> rows;
        // Semiesfera de arriba (del polo al ecuador) y de abajo (del ecuador al polo)
        for (int hemisphere = 0; hemisphere < 2; hemisphere++) {
            for (int r = 0; r <= rings; r++) {
                const float t = static_cast<float>(r) / static_cast<float>(rings);
                const float theta = (static_cast<float>(hemisphere) + t) * kPi * 0.5f;
                const bool pole = (hemisphere == 0 && r == 0) || (hemisphere == 1 && r == rings);
                const float offset = hemisphere == 0 ? h : -h;
                const float y = std::cos(theta) * radius + offset;
                rows.push_back({pole ? 0.0f : std::sin(theta) * radius,
                                y,
                                {std::sin(theta), std::cos(theta)},
                                (y + totalHeight * 0.5f) / totalHeight});
            }
        }
        MeshData mesh;
        revolve(mesh, rows, segments, color);
        return mesh;
    }
}
