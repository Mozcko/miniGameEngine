#include <mge/procgen/MeshData.h>
#include <mge/core/Log.h>

#include <fstream>
#include <sstream>

namespace mge {

    std::uint32_t MeshData::addVertex(const glm::vec3& position, const glm::vec3& normal,
                                      const glm::vec4& color, const glm::vec2& uv) {
        positions.push_back(position);
        normals.push_back(normal);
        colors.push_back(color);
        uvs.push_back(uv);
        return static_cast<std::uint32_t>(positions.size() - 1);
    }

    void MeshData::addTriangle(std::uint32_t a, std::uint32_t b, std::uint32_t c) {
        indices.insert(indices.end(), {a, b, c});
    }

    void MeshData::addQuad(std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t d) {
        indices.insert(indices.end(), {a, b, c, a, c, d});
    }

    void MeshData::reserve(std::size_t vertices, std::size_t triangles) {
        positions.reserve(vertices);
        normals.reserve(vertices);
        colors.reserve(vertices);
        uvs.reserve(vertices);
        indices.reserve(triangles * 3);
    }

    void MeshData::clear() {
        positions.clear();
        normals.clear();
        colors.clear();
        uvs.clear();
        indices.clear();
    }

    Aabb MeshData::bounds() const {
        Aabb box;
        for (const glm::vec3& p : positions) box.expand(p);
        return box;
    }

    std::string MeshData::validate() const {
        const std::size_t n = positions.size();
        if (normals.size() != n || colors.size() != n || uvs.size() != n) {
            std::ostringstream error;
            error << "atributos de distinto largo: positions=" << n << " normals=" << normals.size()
                  << " colors=" << colors.size() << " uvs=" << uvs.size();
            return error.str();
        }
        if (indices.size() % 3 != 0) return "la cantidad de índices no es múltiplo de 3";
        for (std::size_t i = 0; i < indices.size(); i++) {
            if (indices[i] >= n) {
                std::ostringstream error;
                error << "índice " << indices[i] << " fuera de rango en la posición " << i
                      << " (vértices: " << n << ")";
                return error.str();
            }
        }
        return {};
    }

    bool writeObj(const MeshData& mesh, const std::filesystem::path& path) {
        std::ofstream file(path);
        if (!file) {
            MGE_LOG_ERROR("Procgen", "No se pudo escribir ", path.string());
            return false;
        }
        file << "# miniGameEngine MeshData: " << mesh.vertexCount() << " vértices, " << mesh.triangleCount()
             << " triángulos\n";
        for (std::size_t i = 0; i < mesh.vertexCount(); i++) {
            const glm::vec3& p = mesh.positions[i];
            const glm::vec4& c = mesh.colors[i];
            // Extensión común de OBJ: color por vértice después de la posición
            file << "v " << p.x << ' ' << p.y << ' ' << p.z << ' ' << c.r << ' ' << c.g << ' ' << c.b << '\n';
        }
        for (const glm::vec2& uv : mesh.uvs) file << "vt " << uv.x << ' ' << uv.y << '\n';
        for (const glm::vec3& n : mesh.normals) file << "vn " << n.x << ' ' << n.y << ' ' << n.z << '\n';
        for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
            file << 'f';
            for (int k = 0; k < 3; k++) {
                const std::uint32_t index =
                    mesh.indices[i + static_cast<std::size_t>(k)] + 1; // OBJ cuenta desde 1
                file << ' ' << index << '/' << index << '/' << index;
            }
            file << '\n';
        }
        return static_cast<bool>(file);
    }
}
