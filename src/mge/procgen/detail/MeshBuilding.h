#pragma once
// Uso interno de mge_procgen (no es API pública): ayudas compartidas por primitivas y operaciones.

#include <mge/procgen/MeshData.h>

#include <cstdint>
#include <vector>

namespace mge::detail {

    // Agrega el triángulo con el orden que hace que su normal geométrica coincida con las normales
    // de sus vértices (sentido antihorario visto desde el lado al que apuntan las normales)
    void addOrientedTriangle(MeshData& mesh, std::uint32_t a, std::uint32_t b, std::uint32_t c);
    void addOrientedQuad(MeshData& mesh, std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t d);

    // Una fila de una superficie de revolución alrededor del eje Y
    struct Row {
        float radius;     // Distancia al eje (0 = polo)
        float y;          // Altura
        glm::vec2 normal; // (componente radial, componente Y) de la normal
        float v;          // Coordenada de textura vertical
    };

    // Revoluciona filas alrededor del eje Y. Cada fila tiene segments + 1 vértices (el último duplica
    // el primero para la costura de UV). Entre dos filas: quads, o un solo triángulo si una de ellas
    // es un polo (radio 0).
    void revolve(MeshData& mesh, const std::vector<Row>& rows, int segments, const glm::vec4& color);
}
