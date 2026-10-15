#include <mge/procgen/Sdf.h>
#include <mge/procgen/detail/MeshBuilding.h>

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace mge::sdf {

    // --- Primitivas (fórmulas clásicas de Íñigo Quílez) ---

    Sdf sphere(float radius) {
        return [radius](const glm::vec3& p) { return glm::length(p) - radius; };
    }

    Sdf box(const glm::vec3& halfExtents) {
        return [halfExtents](const glm::vec3& p) {
            const glm::vec3 q = glm::abs(p) - halfExtents;
            return glm::length(glm::max(q, glm::vec3(0.0f))) +
                   std::min(std::max(q.x, std::max(q.y, q.z)), 0.0f);
        };
    }

    Sdf roundBox(const glm::vec3& halfExtents, float cornerRadius) {
        return opRound(box(halfExtents - glm::vec3(cornerRadius)), cornerRadius);
    }

    Sdf torus(float majorRadius, float minorRadius) {
        return [majorRadius, minorRadius](const glm::vec3& p) {
            const glm::vec2 q(glm::length(glm::vec2(p.x, p.z)) - majorRadius, p.y);
            return glm::length(q) - minorRadius;
        };
    }

    Sdf capsule(const glm::vec3& a, const glm::vec3& b, float radius) {
        return [a, b, radius](const glm::vec3& p) {
            const glm::vec3 pa = p - a, ba = b - a;
            const float h = glm::clamp(glm::dot(pa, ba) / glm::dot(ba, ba), 0.0f, 1.0f);
            return glm::length(pa - ba * h) - radius;
        };
    }

    Sdf cylinder(float radius, float halfHeight) {
        return [radius, halfHeight](const glm::vec3& p) {
            const glm::vec2 d =
                glm::abs(glm::vec2(glm::length(glm::vec2(p.x, p.z)), p.y)) - glm::vec2(radius, halfHeight);
            return std::min(std::max(d.x, d.y), 0.0f) + glm::length(glm::max(d, glm::vec2(0.0f)));
        };
    }

    Sdf plane(const glm::vec3& normal, float offset) {
        const glm::vec3 n = glm::normalize(normal);
        return [n, offset](const glm::vec3& p) { return glm::dot(n, p) + offset; };
    }

    // --- Booleanas ---

    Sdf opUnion(Sdf a, Sdf b) {
        return [a = std::move(a), b = std::move(b)](const glm::vec3& p) { return std::min(a(p), b(p)); };
    }

    Sdf opSubtract(Sdf a, Sdf b) {
        return [a = std::move(a), b = std::move(b)](const glm::vec3& p) { return std::max(a(p), -b(p)); };
    }

    Sdf opIntersect(Sdf a, Sdf b) {
        return [a = std::move(a), b = std::move(b)](const glm::vec3& p) { return std::max(a(p), b(p)); };
    }

    Sdf opSmoothUnion(Sdf a, Sdf b, float k) {
        return [a = std::move(a), b = std::move(b), k](const glm::vec3& p) {
            const float da = a(p), db = b(p);
            if (k <= 0.0f) return std::min(da, db);
            const float h = glm::clamp(0.5f + 0.5f * (db - da) / k, 0.0f, 1.0f);
            return glm::mix(db, da, h) - k * h * (1.0f - h);
        };
    }

    Sdf opSmoothSubtract(Sdf a, Sdf b, float k) {
        return [a = std::move(a), b = std::move(b), k](const glm::vec3& p) {
            const float da = a(p), db = b(p);
            if (k <= 0.0f) return std::max(da, -db);
            const float h = glm::clamp(0.5f - 0.5f * (da + db) / k, 0.0f, 1.0f);
            return glm::mix(da, -db, h) + k * h * (1.0f - h);
        };
    }

    Sdf opSmoothIntersect(Sdf a, Sdf b, float k) {
        return [a = std::move(a), b = std::move(b), k](const glm::vec3& p) {
            const float da = a(p), db = b(p);
            if (k <= 0.0f) return std::max(da, db);
            const float h = glm::clamp(0.5f - 0.5f * (db - da) / k, 0.0f, 1.0f);
            return glm::mix(db, da, h) + k * h * (1.0f - h);
        };
    }

    // --- Transformaciones ---

    Sdf opTranslate(Sdf a, const glm::vec3& offset) {
        return [a = std::move(a), offset](const glm::vec3& p) { return a(p - offset); };
    }

    Sdf opRotate(Sdf a, const glm::quat& rotation) {
        const glm::quat inverse = glm::inverse(rotation);
        return [a = std::move(a), inverse](const glm::vec3& p) { return a(inverse * p); };
    }

    Sdf opScale(Sdf a, float scale) {
        return [a = std::move(a), scale](const glm::vec3& p) { return a(p / scale) * scale; };
    }

    Sdf opRepeat(Sdf a, const glm::vec3& period) {
        return [a = std::move(a), period](const glm::vec3& p) {
            glm::vec3 q = p;
            for (int axis = 0; axis < 3; axis++) {
                if (period[axis] > 0.0f)
                    q[axis] = p[axis] - period[axis] * std::round(p[axis] / period[axis]);
            }
            return a(q);
        };
    }

    Sdf opTwist(Sdf a, float radiansPerUnit) {
        return [a = std::move(a), radiansPerUnit](const glm::vec3& p) {
            const float angle = radiansPerUnit * p.y;
            const float c = std::cos(angle), s = std::sin(angle);
            return a(glm::vec3(c * p.x - s * p.z, p.y, s * p.x + c * p.z));
        };
    }

    Sdf opRound(Sdf a, float radius) {
        return [a = std::move(a), radius](const glm::vec3& p) { return a(p) - radius; };
    }

    glm::vec3 normal(const Sdf& sdf, const glm::vec3& p, float epsilon) {
        const glm::vec3 gradient(sdf(p + glm::vec3(epsilon, 0, 0)) - sdf(p - glm::vec3(epsilon, 0, 0)),
                                 sdf(p + glm::vec3(0, epsilon, 0)) - sdf(p - glm::vec3(0, epsilon, 0)),
                                 sdf(p + glm::vec3(0, 0, epsilon)) - sdf(p - glm::vec3(0, 0, epsilon)));
        const float length = glm::length(gradient);
        return length > 0.0f ? gradient / length : glm::vec3(0.0f, 1.0f, 0.0f);
    }

    // --- Surface nets ---

    MeshData surfaceNets(const Sdf& sdf, const Aabb& bounds, int resolution, const glm::vec4& color) {
        MeshData mesh;
        if (bounds.isEmpty()) return mesh;
        const glm::vec3 size = bounds.size();
        const float longest = std::max(size.x, std::max(size.y, size.z));
        const float cell = longest / static_cast<float>(std::max(resolution, 2));
        const glm::ivec3 cells = glm::max(glm::ivec3(glm::ceil(size / cell)), glm::ivec3(2));
        const glm::ivec3 points = cells + 1;

        auto pointIndex = [&](int x, int y, int z) {
            return static_cast<std::size_t>((z * points.y + y) * points.x + x);
        };
        auto cellIndex = [&](int x, int y, int z) {
            return static_cast<std::size_t>((z * cells.y + y) * cells.x + x);
        };
        auto position = [&](int x, int y, int z) {
            return bounds.min +
                   glm::vec3(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)) * cell;
        };

        // 1. Muestrear la SDF en todos los puntos de la grilla
        std::vector<float> values(static_cast<std::size_t>(points.x) * points.y * points.z);
        for (int z = 0; z < points.z; z++)
            for (int y = 0; y < points.y; y++)
                for (int x = 0; x < points.x; x++) values[pointIndex(x, y, z)] = sdf(position(x, y, z));

        // 2. Un vértice por celda que la superficie cruza: promedio de los cruces en sus 12 aristas
        constexpr std::uint32_t kNone = 0xFFFFFFFFu;
        std::vector<std::uint32_t> cellVertex(static_cast<std::size_t>(cells.x) * cells.y * cells.z, kNone);
        static const int kCorners[8][3] = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {1, 1, 0},
                                           {0, 0, 1}, {1, 0, 1}, {0, 1, 1}, {1, 1, 1}};
        static const int kEdges[12][2] = {{0, 1}, {2, 3}, {4, 5}, {6, 7}, {0, 2}, {1, 3},
                                          {4, 6}, {5, 7}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
        for (int z = 0; z < cells.z; z++) {
            for (int y = 0; y < cells.y; y++) {
                for (int x = 0; x < cells.x; x++) {
                    float v[8];
                    glm::vec3 p[8];
                    int inside = 0;
                    for (int c = 0; c < 8; c++) {
                        const int px = x + kCorners[c][0], py = y + kCorners[c][1], pz = z + kCorners[c][2];
                        v[c] = values[pointIndex(px, py, pz)];
                        p[c] = position(px, py, pz);
                        if (v[c] < 0.0f) inside++;
                    }
                    if (inside == 0 || inside == 8) continue;
                    glm::vec3 sum(0.0f);
                    int crossings = 0;
                    for (const auto& edge : kEdges) {
                        const float a = v[edge[0]], b = v[edge[1]];
                        if ((a < 0.0f) == (b < 0.0f)) continue;
                        const float t = a / (a - b);
                        sum += glm::mix(p[edge[0]], p[edge[1]], t);
                        crossings++;
                    }
                    const glm::vec3 vertex = sum / static_cast<float>(crossings);
                    cellVertex[cellIndex(x, y, z)] = mesh.addVertex(vertex, normal(sdf, vertex, cell * 0.5f),
                                                                    color, glm::vec2(vertex.x, vertex.z));
                }
            }
        }

        // 3. Un quad por arista de la grilla con cambio de signo, uniendo las 4 celdas que la rodean
        auto quadAround = [&](int axis, int x, int y, int z) {
            // Las otras dos direcciones del eje
            const int u = (axis + 1) % 3, w = (axis + 2) % 3;
            glm::ivec3 base(x, y, z);
            std::uint32_t corners[4];
            int k = 0;
            for (int du = -1; du <= 0; du++) {
                for (int dw = -1; dw <= 0; dw++) {
                    glm::ivec3 c = base;
                    c[u] += du;
                    c[w] += dw;
                    if (c.x < 0 || c.y < 0 || c.z < 0 || c.x >= cells.x || c.y >= cells.y || c.z >= cells.z)
                        return;
                    corners[k++] = cellVertex[cellIndex(c.x, c.y, c.z)];
                }
            }
            for (std::uint32_t index : corners)
                if (index == kNone) return;
            // Orden en ciclo alrededor de la arista: (−,−), (−,0), (0,0), (0,−)
            detail::addOrientedQuad(mesh, corners[0], corners[1], corners[3], corners[2]);
        };
        for (int z = 0; z < points.z; z++) {
            for (int y = 0; y < points.y; y++) {
                for (int x = 0; x < points.x; x++) {
                    const bool here = values[pointIndex(x, y, z)] < 0.0f;
                    if (x + 1 < points.x && here != (values[pointIndex(x + 1, y, z)] < 0.0f))
                        quadAround(0, x, y, z);
                    if (y + 1 < points.y && here != (values[pointIndex(x, y + 1, z)] < 0.0f))
                        quadAround(1, x, y, z);
                    if (z + 1 < points.z && here != (values[pointIndex(x, y, z + 1)] < 0.0f))
                        quadAround(2, x, y, z);
                }
            }
        }
        return mesh;
    }
}
