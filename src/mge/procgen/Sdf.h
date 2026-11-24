#pragma once

#include <mge/core/math/Geometry.h>
#include <mge/procgen/MeshData.h>

#include <functional>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace mge::sdf {

    // Signed distance function: negative inside, 0 on the surface, positive outside.
    // Composed with op* operators to describe shapes using pure math.
    using Sdf = std::function<float(const glm::vec3&)>;

    // --- Primitives (centered at the origin) ---
    Sdf sphere(float radius);
    Sdf box(const glm::vec3& halfExtents);
    Sdf roundBox(const glm::vec3& halfExtents, float cornerRadius);
    Sdf torus(float majorRadius, float minorRadius); // In the XZ plane
    Sdf capsule(const glm::vec3& a, const glm::vec3& b, float radius);
    Sdf cylinder(float radius, float halfHeight);     // Y axis, with caps
    Sdf plane(const glm::vec3& normal, float offset); // dot(n, p) + offset (half-space)

    // --- Booleans ---
    Sdf opUnion(Sdf a, Sdf b);
    Sdf opSubtract(Sdf a, Sdf b); // a minus b
    Sdf opIntersect(Sdf a, Sdf b);
    // Smooth variants: k = blend radius (0 = hard)
    Sdf opSmoothUnion(Sdf a, Sdf b, float k);
    Sdf opSmoothSubtract(Sdf a, Sdf b, float k);
    Sdf opSmoothIntersect(Sdf a, Sdf b, float k);

    // --- Transforms (applied to the query point via the inverse transform) ---
    Sdf opTranslate(Sdf a, const glm::vec3& offset);
    Sdf opRotate(Sdf a, const glm::quat& rotation);
    Sdf opScale(Sdf a, float scale);              // Uniform (preserves exact distances)
    Sdf opRepeat(Sdf a, const glm::vec3& period); // Infinite repetition per axis (0 = no repeat)
    Sdf opTwist(Sdf a, float radiansPerUnit);     // Twist around Y (approximate distance)
    Sdf opRound(Sdf a, float radius);             // Thickens and rounds

    // Surface normal at p (normalized central-difference gradient)
    glm::vec3 normal(const Sdf& sdf, const glm::vec3& p, float epsilon = 1e-3f);

    // Zero level set -> mesh via "naive surface nets": one vertex per cell crossed by the
    // surface (average of its edge crossings) and one quad per sign-changing grid edge.
    // Watertight when the shape lies completely inside bounds.
    // resolution = cells along the longest axis of bounds.
    MeshData surfaceNets(const Sdf& sdf, const Aabb& bounds, int resolution = 64,
                         const glm::vec4& color = glm::vec4(1.0f));
}
