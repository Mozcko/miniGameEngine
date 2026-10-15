#include <mge/procgen/MeshOps.h>
#include <mge/procgen/detail/MeshBuilding.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <tuple>

#include <glm/gtc/matrix_inverse.hpp>

namespace mge {

    using detail::addOrientedQuad;
    using detail::addOrientedTriangle;

    namespace {
        // Área con signo (positiva = antihorario)
        float signedArea(const std::vector<glm::vec2>& polygon) {
            float area = 0.0f;
            for (std::size_t i = 0; i < polygon.size(); i++) {
                const glm::vec2& a = polygon[i];
                const glm::vec2& b = polygon[(i + 1) % polygon.size()];
                area += a.x * b.y - b.x * a.y;
            }
            return area * 0.5f;
        }

        float cross2(const glm::vec2& a, const glm::vec2& b) {
            return a.x * b.y - a.y * b.x;
        }

        bool pointInTriangle(const glm::vec2& p, const glm::vec2& a, const glm::vec2& b, const glm::vec2& c) {
            const float d1 = cross2(b - a, p - a), d2 = cross2(c - b, p - b), d3 = cross2(a - c, p - c);
            return d1 >= 0.0f && d2 >= 0.0f && d3 >= 0.0f;
        }

        // Normal hacia afuera de cada vértice de un polígono antihorario (promedio de sus dos aristas)
        std::vector<glm::vec2> polygonVertexNormals(const std::vector<glm::vec2>& ccw) {
            const std::size_t n = ccw.size();
            std::vector<glm::vec2> normals(n);
            for (std::size_t i = 0; i < n; i++) {
                const glm::vec2 prevEdge = ccw[i] - ccw[(i + n - 1) % n];
                const glm::vec2 nextEdge = ccw[(i + 1) % n] - ccw[i];
                // En un polígono antihorario la normal exterior de la arista (dx, dy) es (dy, -dx)
                const glm::vec2 a = glm::normalize(glm::vec2(prevEdge.y, -prevEdge.x));
                const glm::vec2 b = glm::normalize(glm::vec2(nextEdge.y, -nextEdge.x));
                normals[i] = glm::normalize(a + b);
            }
            return normals;
        }

        std::vector<glm::vec2> counterClockwise(std::vector<glm::vec2> polygon) {
            if (signedArea(polygon) < 0.0f) std::reverse(polygon.begin(), polygon.end());
            return polygon;
        }
    }

    // --- Transformar y combinar ---

    void transformMesh(MeshData& mesh, const glm::mat4& matrix) {
        const glm::mat3 normalMatrix = glm::inverseTranspose(glm::mat3(matrix));
        for (glm::vec3& p : mesh.positions) p = glm::vec3(matrix * glm::vec4(p, 1.0f));
        for (glm::vec3& n : mesh.normals) {
            const glm::vec3 transformed = normalMatrix * n;
            if (glm::length(transformed) > 0.0f) n = glm::normalize(transformed);
        }
        if (glm::determinant(glm::mat3(matrix)) < 0.0f) {
            for (std::size_t t = 0; t + 2 < mesh.indices.size(); t += 3)
                std::swap(mesh.indices[t + 1], mesh.indices[t + 2]);
        }
    }

    void appendMesh(MeshData& dst, const MeshData& src) {
        const auto offset = static_cast<std::uint32_t>(dst.vertexCount());
        dst.positions.insert(dst.positions.end(), src.positions.begin(), src.positions.end());
        dst.normals.insert(dst.normals.end(), src.normals.begin(), src.normals.end());
        dst.colors.insert(dst.colors.end(), src.colors.begin(), src.colors.end());
        dst.uvs.insert(dst.uvs.end(), src.uvs.begin(), src.uvs.end());
        dst.indices.reserve(dst.indices.size() + src.indices.size());
        for (std::uint32_t index : src.indices) dst.indices.push_back(index + offset);
    }

    MeshData mergeMeshes(const MeshData& a, const MeshData& b) {
        MeshData result = a;
        appendMesh(result, b);
        return result;
    }

    void weldVertices(MeshData& mesh, float epsilon) {
        auto q = [epsilon](float value) { return std::lround(value / epsilon); };
        using Key = std::tuple<long, long, long, long, long, long, long, long, long, long, long, long>;
        std::map<Key, std::uint32_t> unique;
        MeshData welded;
        welded.reserve(mesh.vertexCount(), mesh.triangleCount());
        std::vector<std::uint32_t> remap(mesh.vertexCount());
        for (std::size_t i = 0; i < mesh.vertexCount(); i++) {
            const glm::vec3& p = mesh.positions[i];
            const glm::vec3& n = mesh.normals[i];
            const glm::vec4& c = mesh.colors[i];
            const glm::vec2& uv = mesh.uvs[i];
            const Key key{q(p.x), q(p.y), q(p.z), q(n.x), q(n.y),  q(n.z),
                          q(c.r), q(c.g), q(c.b), q(c.a), q(uv.x), q(uv.y)};
            auto it = unique.find(key);
            if (it == unique.end()) it = unique.emplace(key, welded.addVertex(p, n, c, uv)).first;
            remap[i] = it->second;
        }
        for (std::uint32_t index : mesh.indices) welded.indices.push_back(remap[index]);
        mesh = std::move(welded);
    }

    void flipWinding(MeshData& mesh) {
        for (std::size_t t = 0; t + 2 < mesh.indices.size(); t += 3)
            std::swap(mesh.indices[t + 1], mesh.indices[t + 2]);
        for (glm::vec3& n : mesh.normals) n = -n;
    }

    void recomputeNormals(MeshData& mesh, NormalMode mode) {
        if (mode == NormalMode::Flat) {
            MeshData flat;
            flat.reserve(mesh.indices.size(), mesh.triangleCount());
            for (std::size_t t = 0; t + 2 < mesh.indices.size(); t += 3) {
                const std::uint32_t v[3] = {mesh.indices[t], mesh.indices[t + 1], mesh.indices[t + 2]};
                glm::vec3 face = glm::cross(mesh.positions[v[1]] - mesh.positions[v[0]],
                                            mesh.positions[v[2]] - mesh.positions[v[0]]);
                face = glm::length(face) > 0.0f ? glm::normalize(face) : glm::vec3(0.0f, 1.0f, 0.0f);
                std::uint32_t out[3];
                for (int k = 0; k < 3; k++)
                    out[k] = flat.addVertex(mesh.positions[v[k]], face, mesh.colors[v[k]], mesh.uvs[v[k]]);
                flat.addTriangle(out[0], out[1], out[2]);
            }
            mesh = std::move(flat);
            return;
        }
        // Smooth: acumular la normal de cada cara (su largo es 2 × área) en todos los vértices que
        // comparten posición, así las costuras de UV no quedan marcadas
        std::map<std::tuple<long, long, long>, glm::vec3> accumulated;
        auto key = [](const glm::vec3& p) {
            return std::make_tuple(std::lround(p.x * 1e5f), std::lround(p.y * 1e5f), std::lround(p.z * 1e5f));
        };
        for (std::size_t t = 0; t + 2 < mesh.indices.size(); t += 3) {
            const auto a = mesh.indices[t], b = mesh.indices[t + 1], c = mesh.indices[t + 2];
            const glm::vec3 face =
                glm::cross(mesh.positions[b] - mesh.positions[a], mesh.positions[c] - mesh.positions[a]);
            for (auto v : {a, b, c}) accumulated[key(mesh.positions[v])] += face;
        }
        for (std::size_t i = 0; i < mesh.vertexCount(); i++) {
            const glm::vec3 sum = accumulated[key(mesh.positions[i])];
            mesh.normals[i] = glm::length(sum) > 0.0f ? glm::normalize(sum) : glm::vec3(0.0f, 1.0f, 0.0f);
        }
    }

    // --- Perfiles 2D ---

    std::vector<std::uint32_t> triangulatePolygon(const std::vector<glm::vec2>& input) {
        std::vector<std::uint32_t> triangles;
        const std::size_t n = input.size();
        if (n < 3) return triangles;

        // Trabajar en antihorario; los índices se refieren siempre al polígono original
        std::vector<std::uint32_t> remaining(n);
        for (std::size_t i = 0; i < n; i++) remaining[i] = static_cast<std::uint32_t>(i);
        if (signedArea(input) < 0.0f) std::reverse(remaining.begin(), remaining.end());

        std::size_t guard = 0;
        while (remaining.size() > 3 && guard++ < n * n) {
            bool clipped = false;
            for (std::size_t i = 0; i < remaining.size(); i++) {
                const std::uint32_t prev = remaining[(i + remaining.size() - 1) % remaining.size()];
                const std::uint32_t curr = remaining[i];
                const std::uint32_t next = remaining[(i + 1) % remaining.size()];
                const glm::vec2 &a = input[prev], &b = input[curr], &c = input[next];
                if (cross2(b - a, c - b) <= 0.0f) continue; // Vértice reflejo (cóncavo): no es oreja
                bool containsOther = false;
                for (std::uint32_t other : remaining) {
                    if (other == prev || other == curr || other == next) continue;
                    if (pointInTriangle(input[other], a, b, c)) {
                        containsOther = true;
                        break;
                    }
                }
                if (containsOther) continue;
                triangles.insert(triangles.end(), {prev, curr, next});
                remaining.erase(remaining.begin() + static_cast<std::ptrdiff_t>(i));
                clipped = true;
                break;
            }
            if (!clipped) break; // Polígono degenerado o con auto-intersecciones
        }
        if (remaining.size() == 3)
            triangles.insert(triangles.end(), {remaining[0], remaining[1], remaining[2]});
        return triangles;
    }

    MeshData extrude(const std::vector<glm::vec2>& polygon, float depth, const glm::vec4& color) {
        MeshData mesh;
        if (polygon.size() < 3) return mesh;
        const std::vector<glm::vec2> ccw = counterClockwise(polygon);
        const std::size_t n = ccw.size();
        const float h = depth * 0.5f;
        const std::vector<std::uint32_t> capTriangles = triangulatePolygon(ccw);

        // Tapas: frente (+Z) y atrás (-Z)
        for (int side = 0; side < 2; side++) {
            const float z = side == 0 ? h : -h;
            const glm::vec3 normal(0.0f, 0.0f, side == 0 ? 1.0f : -1.0f);
            const auto base = static_cast<std::uint32_t>(mesh.vertexCount());
            for (const glm::vec2& p : ccw) mesh.addVertex({p.x, p.y, z}, normal, color, p);
            for (std::size_t t = 0; t + 2 < capTriangles.size(); t += 3) {
                addOrientedTriangle(mesh, base + capTriangles[t], base + capTriangles[t + 1],
                                    base + capTriangles[t + 2]);
            }
        }
        // Lados: un quad por arista con su propia normal (aristas duras)
        for (std::size_t i = 0; i < n; i++) {
            const glm::vec2 a = ccw[i], b = ccw[(i + 1) % n];
            const glm::vec2 edge = b - a;
            const glm::vec3 normal = glm::normalize(glm::vec3(edge.y, -edge.x, 0.0f));
            const auto v0 = mesh.addVertex({a.x, a.y, h}, normal, color, {0.0f, 1.0f});
            const auto v1 = mesh.addVertex({b.x, b.y, h}, normal, color, {1.0f, 1.0f});
            const auto v2 = mesh.addVertex({b.x, b.y, -h}, normal, color, {1.0f, 0.0f});
            const auto v3 = mesh.addVertex({a.x, a.y, -h}, normal, color, {0.0f, 0.0f});
            addOrientedQuad(mesh, v0, v1, v2, v3);
        }
        return mesh;
    }

    MeshData lathe(const std::vector<glm::vec2>& profile, int segments, const glm::vec4& color) {
        MeshData mesh;
        if (profile.size() < 2) return mesh;
        segments = std::max(segments, 3);
        std::vector<detail::Row> rows;
        float totalLength = 0.0f;
        std::vector<float> lengths(profile.size(), 0.0f);
        for (std::size_t i = 1; i < profile.size(); i++) {
            totalLength += glm::length(profile[i] - profile[i - 1]);
            lengths[i] = totalLength;
        }
        for (std::size_t i = 0; i < profile.size(); i++) {
            // Tangente por diferencias centradas; normal = tangente girada (x = radial, y = vertical)
            const glm::vec2 prev = profile[i == 0 ? 0 : i - 1];
            const glm::vec2 next = profile[std::min(i + 1, profile.size() - 1)];
            glm::vec2 tangent = next - prev;
            glm::vec2 normal = glm::length(tangent) > 0.0f ? glm::normalize(glm::vec2(tangent.y, -tangent.x))
                                                           : glm::vec2(1.0f, 0.0f);
            const float v = totalLength > 0.0f ? lengths[i] / totalLength : 0.0f;
            rows.push_back({std::max(profile[i].x, 0.0f), profile[i].y, normal, v});
        }
        detail::revolve(mesh, rows, segments, color);
        return mesh;
    }

    MeshData sweep(const std::vector<glm::vec2>& profile, const std::vector<glm::vec3>& path, bool closedPath,
                   bool caps, const glm::vec4& color) {
        MeshData mesh;
        if (profile.size() < 3 || path.size() < 2) return mesh;
        const std::vector<glm::vec2> ccw = counterClockwise(profile);
        const std::vector<glm::vec2> profileNormals = polygonVertexNormals(ccw);
        const std::size_t ringSize = ccw.size();
        const std::size_t count = path.size();

        // Tangente en cada punto del camino
        std::vector<glm::vec3> tangents(count);
        for (std::size_t i = 0; i < count; i++) {
            glm::vec3 prev = path[i == 0 ? (closedPath ? count - 1 : 0) : i - 1];
            glm::vec3 next = path[i + 1 == count ? (closedPath ? 0 : i) : i + 1];
            tangents[i] = glm::normalize(next - prev);
        }
        // Marco inicial: un vector perpendicular cualquiera a la primera tangente
        glm::vec3 helper = std::abs(tangents[0].y) < 0.99f ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
        std::vector<glm::vec3> normals(count), binormals(count);
        normals[0] = glm::normalize(glm::cross(glm::cross(tangents[0], helper), tangents[0]));
        // Transporte paralelo: rotar el marco con el mínimo giro que lleva una tangente a la siguiente
        for (std::size_t i = 1; i < count; i++) {
            glm::vec3 axis = glm::cross(tangents[i - 1], tangents[i]);
            glm::vec3 n = normals[i - 1];
            const float sinAngle = glm::length(axis);
            if (sinAngle > 1e-6f) {
                axis /= sinAngle;
                const float cosAngle = glm::clamp(glm::dot(tangents[i - 1], tangents[i]), -1.0f, 1.0f);
                // Rodrigues
                n = n * cosAngle + glm::cross(axis, n) * sinAngle +
                    axis * glm::dot(axis, n) * (1.0f - cosAngle);
            }
            normals[i] = glm::normalize(n - tangents[i] * glm::dot(n, tangents[i]));
        }
        for (std::size_t i = 0; i < count; i++) binormals[i] = glm::cross(tangents[i], normals[i]);

        // Anillos: el perfil (x, y) va sobre (normal, binormal) de cada punto
        for (std::size_t i = 0; i < count; i++) {
            const float v = static_cast<float>(i) / static_cast<float>(count - 1);
            for (std::size_t k = 0; k <= ringSize; k++) {
                const std::size_t j = k % ringSize;
                const glm::vec3 position = path[i] + normals[i] * ccw[j].x + binormals[i] * ccw[j].y;
                const glm::vec3 normal =
                    glm::normalize(normals[i] * profileNormals[j].x + binormals[i] * profileNormals[j].y);
                mesh.addVertex(position, normal, color,
                               {static_cast<float>(k) / static_cast<float>(ringSize), v});
            }
        }
        const auto stride = static_cast<std::uint32_t>(ringSize + 1);
        const std::size_t segmentsAlong = closedPath ? count : count - 1;
        for (std::size_t i = 0; i < segmentsAlong; i++) {
            const auto ringA = static_cast<std::uint32_t>(i) * stride;
            const auto ringB = static_cast<std::uint32_t>((i + 1) % count) * stride;
            for (std::uint32_t k = 0; k < static_cast<std::uint32_t>(ringSize); k++) {
                addOrientedQuad(mesh, ringA + k, ringA + k + 1, ringB + k + 1, ringB + k);
            }
        }
        // Tapas en los extremos
        if (caps && !closedPath) {
            const std::vector<std::uint32_t> capTriangles = triangulatePolygon(ccw);
            for (std::size_t end = 0; end < 2; end++) {
                const std::size_t i = end == 0 ? 0 : count - 1;
                const glm::vec3 capNormal = end == 0 ? -tangents[i] : tangents[i];
                const auto base = static_cast<std::uint32_t>(mesh.vertexCount());
                for (const glm::vec2& p : ccw)
                    mesh.addVertex(path[i] + normals[i] * p.x + binormals[i] * p.y, capNormal, color, p);
                for (std::size_t t = 0; t + 2 < capTriangles.size(); t += 3) {
                    addOrientedTriangle(mesh, base + capTriangles[t], base + capTriangles[t + 1],
                                        base + capTriangles[t + 2]);
                }
            }
        }
        return mesh;
    }
}
