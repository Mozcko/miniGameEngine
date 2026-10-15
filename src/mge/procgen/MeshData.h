#pragma once

#include <mge/core/math/Geometry.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace mge {

    // Malla en CPU, independiente de la GPU (se genera y se testea sin ventana).
    // Arreglos separados (SoA) del mismo largo: cada vértice tiene posición, normal, color y UV.
    // indices: triángulos (3 por triángulo), en sentido antihorario visto desde afuera.
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
        // Dos triángulos: (a, b, c) y (a, c, d)
        void addQuad(std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t d);

        std::size_t vertexCount() const { return positions.size(); }
        std::size_t triangleCount() const { return indices.size() / 3; }
        bool empty() const { return positions.empty(); }

        void reserve(std::size_t vertices, std::size_t triangles);
        void clear();

        Aabb bounds() const;
        // Vacío si la malla es válida; si no, describe el primer problema encontrado
        std::string validate() const;
    };

    // Exporta a Wavefront OBJ (posiciones + color por vértice, UV, normales) para inspeccionar
    // la malla en Blender o cualquier visor. Devuelve false si no pudo escribir el archivo.
    bool writeObj(const MeshData& mesh, const std::filesystem::path& path);
}
