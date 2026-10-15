#pragma once

#include <mge/core/math/Geometry.h>
#include <mge/procgen/MeshData.h>

#include <functional>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace mge::sdf {

    // Función de distancia con signo: negativa adentro, 0 en la superficie, positiva afuera.
    // Se componen con los operadores op* para describir formas con pura matemática.
    using Sdf = std::function<float(const glm::vec3&)>;

    // --- Primitivas (centradas en el origen) ---
    Sdf sphere(float radius);
    Sdf box(const glm::vec3& halfExtents);
    Sdf roundBox(const glm::vec3& halfExtents, float cornerRadius);
    Sdf torus(float majorRadius, float minorRadius); // En el plano XZ
    Sdf capsule(const glm::vec3& a, const glm::vec3& b, float radius);
    Sdf cylinder(float radius, float halfHeight);     // Eje Y, con tapas
    Sdf plane(const glm::vec3& normal, float offset); // dot(n, p) + offset (medio espacio)

    // --- Booleanas ---
    Sdf opUnion(Sdf a, Sdf b);
    Sdf opSubtract(Sdf a, Sdf b); // a sin b
    Sdf opIntersect(Sdf a, Sdf b);
    // Versiones suaves: k = radio de la transición (0 = dura)
    Sdf opSmoothUnion(Sdf a, Sdf b, float k);
    Sdf opSmoothSubtract(Sdf a, Sdf b, float k);
    Sdf opSmoothIntersect(Sdf a, Sdf b, float k);

    // --- Transformaciones (se aplican al punto con la transformación inversa) ---
    Sdf opTranslate(Sdf a, const glm::vec3& offset);
    Sdf opRotate(Sdf a, const glm::quat& rotation);
    Sdf opScale(Sdf a, float scale);              // Uniforme (mantiene distancias correctas)
    Sdf opRepeat(Sdf a, const glm::vec3& period); // Repetición infinita en cada eje (0 = sin repetir)
    Sdf opTwist(Sdf a, float radiansPerUnit);     // Torsión alrededor de Y (distancia aproximada)
    Sdf opRound(Sdf a, float radius);             // Engrosa y redondea

    // Normal en p (gradiente por diferencias centrales, normalizado)
    glm::vec3 normal(const Sdf& sdf, const glm::vec3& p, float epsilon = 1e-3f);

    // Superficie de nivel 0 -> malla, con "naive surface nets": un vértice por celda que cruza la
    // superficie (promedio de los cruces en sus aristas) y un quad por arista de la grilla que cambia
    // de signo. Hermética si la forma queda completamente dentro de bounds.
    // resolution = celdas en el eje más largo de bounds.
    MeshData surfaceNets(const Sdf& sdf, const Aabb& bounds, int resolution = 64,
                         const glm::vec4& color = glm::vec4(1.0f));
}
