#pragma once

#include <mge/procgen/MeshData.h>

#include <glm/glm.hpp>

namespace mge::primitives {

    // Formas básicas generadas por código. Todas centradas en el origen, con normales, UVs y color,
    // y triángulos en sentido antihorario visto desde afuera. Las formas cerradas son herméticas
    // (cada arista la comparten exactamente 2 triángulos, soldando los vértices duplicados de las costuras).

    // Plano en XZ (y = 0) mirando a +Y, subdividido en segX × segZ celdas
    MeshData plane(float width, float depth, int segX = 1, int segZ = 1,
                   const glm::vec4& color = glm::vec4(1.0f));

    // Caja con caras planas (24 vértices, 12 triángulos)
    MeshData box(const glm::vec3& size, const glm::vec4& color = glm::vec4(1.0f));

    // Esfera por latitud/longitud: segments alrededor (>= 3), rings de polo a polo (>= 2)
    MeshData uvSphere(float radius, int segments = 32, int rings = 16,
                      const glm::vec4& color = glm::vec4(1.0f));

    // Esfera geodésica: icosaedro subdividido (vértices compartidos, sin costura de UV)
    MeshData icosphere(float radius, int subdivisions = 2, const glm::vec4& color = glm::vec4(1.0f));

    // Cilindro vertical de altura height (y en [-h/2, h/2]), con tapas opcionales
    MeshData cylinder(float radius, float height, int segments = 32, bool caps = true,
                      const glm::vec4& color = glm::vec4(1.0f));

    // Cono: base en y = -h/2, punta en y = +h/2, con base cerrada
    MeshData cone(float radius, float height, int segments = 32, const glm::vec4& color = glm::vec4(1.0f));

    // Toro en el plano XZ: majorRadius al centro del tubo, minorRadius del tubo
    MeshData torus(float majorRadius, float minorRadius, int segments = 48, int sides = 24,
                   const glm::vec4& color = glm::vec4(1.0f));

    // Cápsula vertical: cilindro de altura height entre dos semiesferas de radio radius
    MeshData capsule(float radius, float height, int segments = 32, int rings = 8,
                     const glm::vec4& color = glm::vec4(1.0f));
}
