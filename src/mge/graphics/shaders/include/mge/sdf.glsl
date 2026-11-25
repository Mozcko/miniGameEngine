// Signed distance field (SDF) library for GLSL.
// Mirrors the functions and formulas in src/mge/procgen/Sdf.h for exact CPU/GPU agreement.
#ifndef MGE_SDF_GLSL
#define MGE_SDF_GLSL

// --- Primitives (centered at the origin) ---

// Sphere of radius r
float sdSphere(vec3 p, float radius) {
    return length(p) - radius;
}

// Box with half-extents halfExtents
float sdBox(vec3 p, vec3 halfExtents) {
    vec3 q = abs(p) - halfExtents;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}

// Thickens or rounds a signed distance by a radius
float opRound(float d, float radius) {
    return d - radius;
}

// Box with rounded corners
float sdRoundBox(vec3 p, vec3 halfExtents, float cornerRadius) {
    return opRound(sdBox(p, halfExtents - vec3(cornerRadius)), cornerRadius);
}

// Torus in the XZ plane
float sdTorus(vec3 p, float majorRadius, float minorRadius) {
    vec2 q = vec2(length(p.xz) - majorRadius, p.y);
    return length(q) - minorRadius;
}

// Capsule between two points a and b
float sdCapsule(vec3 p, vec3 a, vec3 b, float radius) {
    vec3 pa = p - a;
    vec3 ba = b - a;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    return length(pa - ba * h) - radius;
}

// Cylinder along the Y axis with flat caps
float sdCylinder(vec3 p, float radius, float halfHeight) {
    vec2 d = abs(vec2(length(p.xz), p.y)) - vec2(radius, halfHeight);
    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0));
}

// Plane oriented by normal with offset (half-space dot(n, p) + offset)
float sdPlane(vec3 p, vec3 normal, float offset) {
    return dot(normalize(normal), p) + offset;
}

// --- Boolean Operations ---

// Union (minimum)
float opUnion(float d1, float d2) {
    return min(d1, d2);
}

// Subtraction (d1 without d2)
float opSubtract(float d1, float d2) {
    return max(d1, -d2);
}

// Intersection (maximum)
float opIntersect(float d1, float d2) {
    return max(d1, d2);
}

// Smooth union (k = blending radius)
float opSmoothUnion(float d1, float d2, float k) {
    if (k <= 0.0) return min(d1, d2);
    float h = clamp(0.5 + 0.5 * (d2 - d1) / k, 0.0, 1.0);
    return mix(d2, d1, h) - k * h * (1.0 - h);
}

// Smooth subtraction (k = blending radius)
float opSmoothSubtract(float d1, float d2, float k) {
    if (k <= 0.0) return max(d1, -d2);
    float h = clamp(0.5 - 0.5 * (d1 + d2) / k, 0.0, 1.0);
    return mix(d1, -d2, h) + k * h * (1.0 - h);
}

// Smooth intersection (k = blending radius)
float opSmoothIntersect(float d1, float d2, float k) {
    if (k <= 0.0) return max(d1, d2);
    float h = clamp(0.5 - 0.5 * (d2 - d1) / k, 0.0, 1.0);
    return mix(d2, d1, h) + k * h * (1.0 - h);
}

// --- Domain Transformations ---

// Infinite periodic repetition along axes with period > 0
vec3 opRepeat(vec3 p, vec3 period) {
    vec3 q = p;
    if (period.x > 0.0) q.x = p.x - period.x * round(p.x / period.x);
    if (period.y > 0.0) q.y = p.y - period.y * round(p.y / period.y);
    if (period.z > 0.0) q.z = p.z - period.z * round(p.z / period.z);
    return q;
}

// Twist around the Y axis
vec3 opTwist(vec3 p, float radiansPerUnit) {
    float angle = radiansPerUnit * p.y;
    float c = cos(angle);
    float s = sin(angle);
    return vec3(c * p.x - s * p.z, p.y, s * p.x + c * p.z);
}

#endif // MGE_SDF_GLSL
