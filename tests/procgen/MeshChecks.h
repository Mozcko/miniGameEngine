#pragma once
// Verificaciones geométricas reutilizables para los tests de mallas (primitivas, operaciones, SDF)

#include <mge/procgen/MeshData.h>

#include <cmath>
#include <cstdint>
#include <map>
#include <tuple>
#include <utility>
#include <vector>

namespace mge_test {

    // Id canónico por posición (suelda vértices duplicados de costuras y polos)
    inline std::vector<std::uint32_t> weldIds(const mge::MeshData& mesh, float epsilon = 1e-4f) {
        std::map<std::tuple<long, long, long>, std::uint32_t> seen;
        std::vector<std::uint32_t> ids(mesh.vertexCount());
        for (std::size_t i = 0; i < mesh.vertexCount(); i++) {
            const glm::vec3& p = mesh.positions[i];
            auto key = std::make_tuple(std::lround(p.x / epsilon), std::lround(p.y / epsilon),
                                       std::lround(p.z / epsilon));
            auto it = seen.find(key);
            if (it == seen.end()) it = seen.emplace(key, static_cast<std::uint32_t>(seen.size())).first;
            ids[i] = it->second;
        }
        return ids;
    }

    // Hermética y con orientación consistente: tras soldar, cada arista dirigida aparece una sola
    // vez y su opuesta también una sola vez. Los triángulos degenerados (tras soldar) se ignoran.
    inline bool isWatertight(const mge::MeshData& mesh) {
        const std::vector<std::uint32_t> ids = weldIds(mesh);
        std::map<std::pair<std::uint32_t, std::uint32_t>, int> edges;
        for (std::size_t t = 0; t + 2 < mesh.indices.size(); t += 3) {
            const std::uint32_t v[3] = {ids[mesh.indices[t]], ids[mesh.indices[t + 1]],
                                        ids[mesh.indices[t + 2]]};
            if (v[0] == v[1] || v[1] == v[2] || v[0] == v[2]) continue;
            for (int k = 0; k < 3; k++) edges[{v[k], v[(k + 1) % 3]}]++;
        }
        for (const auto& [edge, count] : edges) {
            if (count != 1) return false;
            auto reverse = edges.find({edge.second, edge.first});
            if (reverse == edges.end() || reverse->second != 1) return false;
        }
        return !edges.empty();
    }

    // Cada triángulo (no degenerado) gira de forma que su normal coincide con las de sus vértices
    inline bool windingMatchesNormals(const mge::MeshData& mesh) {
        for (std::size_t t = 0; t + 2 < mesh.indices.size(); t += 3) {
            const auto a = mesh.indices[t], b = mesh.indices[t + 1], c = mesh.indices[t + 2];
            const glm::vec3 face =
                glm::cross(mesh.positions[b] - mesh.positions[a], mesh.positions[c] - mesh.positions[a]);
            if (glm::length(face) < 1e-9f) continue;
            if (glm::dot(face, mesh.normals[a] + mesh.normals[b] + mesh.normals[c]) <= 0.0f) return false;
        }
        return true;
    }

    inline bool normalsAreUnit(const mge::MeshData& mesh, float tolerance = 1e-3f) {
        for (const glm::vec3& n : mesh.normals) {
            if (std::abs(glm::length(n) - 1.0f) > tolerance) return false;
        }
        return true;
    }

    // Para formas convexas centradas en el origen: toda normal apunta hacia afuera
    inline bool normalsPointOutward(const mge::MeshData& mesh) {
        for (std::size_t i = 0; i < mesh.vertexCount(); i++) {
            if (glm::dot(mesh.normals[i], mesh.positions[i]) <= 0.0f) return false;
        }
        return true;
    }
}
