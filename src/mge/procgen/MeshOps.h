#pragma once

#include <mge/procgen/MeshData.h>

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

namespace mge {

    // --- Transformar y combinar ---

    // Aplica la matriz a posiciones y normales (normales con la inversa transpuesta, renormalizadas).
    // Si la matriz refleja (determinante < 0) invierte el orden de los triángulos para que sigan
    // siendo antihorarios vistos desde afuera.
    void transformMesh(MeshData& mesh, const glm::mat4& matrix);

    // Agrega src al final de dst (los índices de src se desplazan)
    void appendMesh(MeshData& dst, const MeshData& src);
    MeshData mergeMeshes(const MeshData& a, const MeshData& b);

    // Une vértices idénticos (posición, normal, color y UV dentro de epsilon). Las aristas duras
    // (normales distintas) y las costuras de UV se conservan.
    void weldVertices(MeshData& mesh, float epsilon = 1e-5f);

    // Invierte el sentido de todos los triángulos y las normales (la malla "mira hacia adentro")
    void flipWinding(MeshData& mesh);

    enum class NormalMode {
        Smooth, // Promedio ponderado por área de las caras que comparten la POSICIÓN (suaviza costuras)
        Flat    // Cada triángulo con sus propios 3 vértices y la normal de la cara (aspecto facetado)
    };
    void recomputeNormals(MeshData& mesh, NormalMode mode);

    // --- Construir a partir de perfiles 2D ---

    // Triangula un polígono simple (sin auto‑intersecciones; puede ser cóncavo) con ear clipping.
    // Acepta orden horario o antihorario. Devuelve índices de triángulos antihorarios.
    std::vector<std::uint32_t> triangulatePolygon(const std::vector<glm::vec2>& polygon);

    // Extruye un polígono del plano XY a lo largo de Z, de -depth/2 a +depth/2, con tapas.
    // Caras laterales planas (aristas duras). Resultado hermético.
    MeshData extrude(const std::vector<glm::vec2>& polygon, float depth,
                     const glm::vec4& color = glm::vec4(1.0f));

    // Torno: revoluciona un perfil (x = radio, y = altura) alrededor del eje Y.
    // Ordenar el perfil de abajo hacia arriba para que las normales miren hacia afuera.
    // Un punto con x = 0 es un polo (cierra la forma).
    MeshData lathe(const std::vector<glm::vec2>& profile, int segments = 32,
                   const glm::vec4& color = glm::vec4(1.0f));

    // Barrido: lleva un perfil cerrado (plano XY) a lo largo de un camino 3D con marcos de transporte
    // paralelo (sin giros bruscos). Con caps = true cierra los extremos (si el camino no es cerrado).
    MeshData sweep(const std::vector<glm::vec2>& profile, const std::vector<glm::vec3>& path,
                   bool closedPath = false, bool caps = true, const glm::vec4& color = glm::vec4(1.0f));
}
