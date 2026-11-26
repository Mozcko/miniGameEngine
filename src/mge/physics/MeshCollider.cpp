#include <mge/physics/MeshCollider.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>

namespace mge {

    namespace {

        constexpr float kEpsilon = 1e-6f;
        constexpr std::uint32_t kLeafMaxTriangles = 4;
        constexpr int kSweepBisectIters = 10;
        constexpr int kMaxSweepSteps = 1024;

        // Closest point on triangle (a, b, c) to point p (Ericson, Real-Time Collision Detection 5.1.5).
        glm::vec3 closestPointOnTriangle(const glm::vec3& p, const glm::vec3& a, const glm::vec3& b,
                                         const glm::vec3& c) {
            const glm::vec3 ab = b - a;
            const glm::vec3 ac = c - a;
            const glm::vec3 ap = p - a;

            const float d1 = glm::dot(ab, ap);
            const float d2 = glm::dot(ac, ap);
            if (d1 <= 0.0f && d2 <= 0.0f) {
                return a;
            }

            const glm::vec3 bp = p - b;
            const float d3 = glm::dot(ab, bp);
            const float d4 = glm::dot(ac, bp);
            if (d3 >= 0.0f && d4 <= d3) {
                return b;
            }

            const float vc = d1 * d4 - d3 * d2;
            if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
                const float v = d1 / (d1 - d3);
                return a + v * ab;
            }

            const glm::vec3 cp = p - c;
            const float d5 = glm::dot(ab, cp);
            const float d6 = glm::dot(ac, cp);
            if (d6 >= 0.0f && d5 <= d6) {
                return c;
            }

            const float vb = d5 * d2 - d1 * d6;
            if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
                const float w = d2 / (d2 - d6);
                return a + w * ac;
            }

            const float va = d3 * d6 - d5 * d4;
            if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
                const float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
                return b + w * (c - b);
            }

            const float denom = 1.0f / (va + vb + vc);
            const float v = vb * denom;
            const float w = vc * denom;
            return a + ab * v + ac * w;
        }

        // Closest points between line segments [p1, q1] and [p2, q2] (Ericson, RTCD 5.1.9).
        void closestPointsSegmentSegment(const glm::vec3& p1, const glm::vec3& q1, const glm::vec3& p2,
                                         const glm::vec3& q2, glm::vec3& outC1, glm::vec3& outC2) {
            const glm::vec3 d1 = q1 - p1;
            const glm::vec3 d2 = q2 - p2;
            const glm::vec3 r = p1 - p2;
            const float a = glm::dot(d1, d1);
            const float e = glm::dot(d2, d2);
            const float f = glm::dot(d2, r);

            float s = 0.0f;
            float t = 0.0f;

            if (a <= kEpsilon && e <= kEpsilon) {
                outC1 = p1;
                outC2 = p2;
                return;
            }
            if (a <= kEpsilon) {
                t = std::clamp(f / e, 0.0f, 1.0f);
            } else {
                const float c = glm::dot(d1, r);
                if (e <= kEpsilon) {
                    s = std::clamp(-c / a, 0.0f, 1.0f);
                } else {
                    const float b = glm::dot(d1, d2);
                    const float denom = a * e - b * b;
                    if (denom > 1e-12f) {
                        s = std::clamp((b * f - c * e) / denom, 0.0f, 1.0f);
                    } else {
                        s = 0.0f;
                    }
                    t = (b * s + f) / e;
                    if (t < 0.0f) {
                        t = 0.0f;
                        s = std::clamp(-c / a, 0.0f, 1.0f);
                    } else if (t > 1.0f) {
                        t = 1.0f;
                        s = std::clamp((b - c) / a, 0.0f, 1.0f);
                    }
                }
            }

            outC1 = p1 + d1 * s;
            outC2 = p2 + d2 * t;
        }

        // Closest points between segment [p0, p1] and triangle (tri.a, tri.b, tri.c).
        void closestPointsSegmentTriangle(const glm::vec3& p0, const glm::vec3& p1,
                                          const CollisionTriangle& tri, glm::vec3& outSeg,
                                          glm::vec3& outTri) {
            const glm::vec3 segDir = p1 - p0;
            const float segLenSq = glm::dot(segDir, segDir);

            // 1. Direct intersection between segment and triangle (distance = 0)
            if (segLenSq > 1e-12f) {
                const float segLen = std::sqrt(segLenSq);
                const Ray segRay(p0, segDir / segLen);
                if (const auto hitT = intersectRayTriangle(segRay, tri.a, tri.b, tri.c)) {
                    if (*hitT >= 0.0f && *hitT <= segLen) {
                        outSeg = segRay.at(*hitT);
                        outTri = outSeg;
                        return;
                    }
                }
            }

            // 2. Segment endpoints to triangle
            glm::vec3 bestSeg = p0;
            glm::vec3 bestTri = closestPointOnTriangle(p0, tri.a, tri.b, tri.c);
            glm::vec3 diff = bestSeg - bestTri;
            float bestDistSq = glm::dot(diff, diff);

            auto consider = [&](const glm::vec3& candSeg, const glm::vec3& candTri) {
                const glm::vec3 d = candSeg - candTri;
                const float distSq = glm::dot(d, d);
                if (distSq < bestDistSq) {
                    bestDistSq = distSq;
                    bestSeg = candSeg;
                    bestTri = candTri;
                }
            };

            if (segLenSq > 1e-12f) {
                consider(p1, closestPointOnTriangle(p1, tri.a, tri.b, tri.c));

                // 3. Segment to each of the 3 triangle edges
                glm::vec3 edgeSeg{0.0f};
                glm::vec3 edgeTri{0.0f};

                closestPointsSegmentSegment(p0, p1, tri.a, tri.b, edgeSeg, edgeTri);
                consider(edgeSeg, edgeTri);

                closestPointsSegmentSegment(p0, p1, tri.b, tri.c, edgeSeg, edgeTri);
                consider(edgeSeg, edgeTri);

                closestPointsSegmentSegment(p0, p1, tri.c, tri.a, edgeSeg, edgeTri);
                consider(edgeSeg, edgeTri);
            }

            outSeg = bestSeg;
            outTri = bestTri;
        }

        ShapeContact contactSphereTriangle(const glm::vec3& center, float testRadius,
                                           const CollisionTriangle& tri, std::uint32_t triIndex) {
            ShapeContact contact;
            const glm::vec3 closest = closestPointOnTriangle(center, tri.a, tri.b, tri.c);
            const glm::vec3 delta = center - closest;
            const float distSq = glm::dot(delta, delta);
            if (distSq >= testRadius * testRadius) {
                return contact;
            }

            contact.hit = true;
            contact.point = closest;
            contact.triangleIndex = triIndex;
            if (distSq > 1e-12f) {
                const float dist = std::sqrt(distSq);
                contact.normal = delta / dist;
                contact.faceNormal =
                    (glm::dot(tri.normal, contact.normal) >= 0.0f) ? tri.normal : -tri.normal;
                contact.penetration = testRadius - dist;
            } else {
                contact.normal = tri.normal;
                contact.faceNormal = tri.normal;
                contact.penetration = testRadius;
            }
            return contact;
        }

        ShapeContact contactCapsuleTriangle(const Capsule& cap, float testRadius,
                                            const CollisionTriangle& tri, std::uint32_t triIndex) {
            ShapeContact contact;
            const glm::vec3 p0 = cap.bottomCenter();
            const glm::vec3 p1 = cap.topCenter();

            glm::vec3 closestSeg{0.0f};
            glm::vec3 closestTri{0.0f};
            closestPointsSegmentTriangle(p0, p1, tri, closestSeg, closestTri);

            const glm::vec3 delta = closestSeg - closestTri;
            const float distSq = glm::dot(delta, delta);
            if (distSq >= testRadius * testRadius) {
                return contact;
            }

            contact.hit = true;
            contact.point = closestTri;
            contact.triangleIndex = triIndex;

            if (distSq > 1e-12f) {
                const float dist = std::sqrt(distSq);
                contact.normal = delta / dist;
                contact.faceNormal =
                    (glm::dot(tri.normal, contact.normal) >= 0.0f) ? tri.normal : -tri.normal;
                contact.penetration = testRadius - dist;
                return contact;
            }

            // Segment intersects the triangle: orient normal towards the capsule center
            glm::vec3 n = tri.normal;
            const glm::vec3 capsuleMid = (p0 + p1) * 0.5f;
            if (glm::dot(capsuleMid - tri.a, n) < 0.0f) {
                n = -n;
            }
            const float d0 = glm::dot(p0 - tri.a, n);
            const float d1 = glm::dot(p1 - tri.a, n);
            const float deepest = std::min(d0, d1);
            contact.normal = n;
            contact.faceNormal = n;
            contact.penetration = testRadius - deepest;
            return contact;
        }

        template <typename FindBlockingFn>
        ShapeCastHit sweepShapeImpl(float shapeRadius, const glm::vec3& delta, float skin,
                                    FindBlockingFn&& findBlockingContact) {
            ShapeCastHit result;
            const float deltaLenSq = glm::dot(delta, delta);
            if (deltaLenSq < 1e-12f) {
                return result;
            }

            const float deltaLen = std::sqrt(deltaLenSq);
            const glm::vec3 moveDir = delta / deltaLen;

            const ShapeContact startContact = findBlockingContact(0.0f, shapeRadius + skin * 0.5f);
            if (startContact.hit) {
                result.hit = true;
                result.fraction = 0.0f;
                result.normal = startContact.normal;
                result.faceNormal = startContact.faceNormal;
                result.point = startContact.point;
                result.triangleIndex = startContact.triangleIndex;
                return result;
            }

            // Sample at intervals <= radius * 0.25 so even a zero-thickness triangle cannot be skipped
            const float maxStepDist = std::max(shapeRadius * 0.25f, 0.02f);
            const int numSteps =
                std::clamp(static_cast<int>(std::ceil(deltaLen / maxStepDist)), 1, kMaxSweepSteps);

            float prevT = 0.0f;
            for (int step = 1; step <= numSteps; ++step) {
                const float currT = static_cast<float>(step) / static_cast<float>(numSteps);
                const ShapeContact c = findBlockingContact(currT, shapeRadius);
                if (!c.hit) {
                    prevT = currT;
                    continue;
                }

                // Refine time of impact on [prevT, currT] via bisection
                float lo = prevT;
                float hi = currT;
                ShapeContact hitContact = c;
                for (int b = 0; b < kSweepBisectIters; ++b) {
                    const float mid = 0.5f * (lo + hi);
                    const ShapeContact midContact = findBlockingContact(mid, shapeRadius);
                    if (midContact.hit) {
                        hi = mid;
                        hitContact = midContact;
                    } else {
                        lo = mid;
                    }
                }

                // Back off along delta so perpendicular separation from the hit plane equals `skin`
                const float approach = std::max(-glm::dot(moveDir, hitContact.normal), 0.25f);
                const float backoffFrac = (skin / approach) / deltaLen;
                result.hit = true;
                result.fraction = std::clamp(hi - backoffFrac, 0.0f, 1.0f);
                result.normal = hitContact.normal;
                result.faceNormal = hitContact.faceNormal;
                result.point = hitContact.point;
                result.triangleIndex = hitContact.triangleIndex;
                return result;
            }

            return result;
        }

    } // namespace

    void MeshCollider::clear() {
        m_triangles.clear();
        m_triIndices.clear();
        m_nodes.clear();
        m_bounds = Aabb{};
    }

    void MeshCollider::addTriangle(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c) {
        const glm::vec3 crossVal = glm::cross(b - a, c - a);
        const float areaSq = glm::dot(crossVal, crossVal);
        if (areaSq <= 1e-12f) {
            return; // Skip degenerate triangle
        }

        CollisionTriangle tri;
        tri.a = a;
        tri.b = b;
        tri.c = c;
        tri.normal = crossVal / std::sqrt(areaSq);
        tri.bounds = Aabb::fromPoints({a, b, c});

        m_triangles.push_back(tri);
        m_bounds.merge(tri.bounds);
        m_nodes.clear(); // Mark BVH dirty until build()
    }

    void MeshCollider::addMesh(const MeshData& mesh, const glm::mat4& transform) {
        const std::size_t triCount = mesh.indices.size() / 3;
        m_triangles.reserve(m_triangles.size() + triCount);
        for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
            const std::uint32_t ia = mesh.indices[i];
            const std::uint32_t ib = mesh.indices[i + 1];
            const std::uint32_t ic = mesh.indices[i + 2];
            if (ia >= mesh.positions.size() || ib >= mesh.positions.size() || ic >= mesh.positions.size()) {
                continue;
            }
            const glm::vec3 a = glm::vec3(transform * glm::vec4(mesh.positions[ia], 1.0f));
            const glm::vec3 b = glm::vec3(transform * glm::vec4(mesh.positions[ib], 1.0f));
            const glm::vec3 c = glm::vec3(transform * glm::vec4(mesh.positions[ic], 1.0f));
            addTriangle(a, b, c);
        }
    }

    void MeshCollider::build() {
        m_nodes.clear();
        m_triIndices.resize(m_triangles.size());
        for (std::size_t i = 0; i < m_triangles.size(); ++i) {
            m_triIndices[i] = static_cast<std::uint32_t>(i);
        }
        if (m_triangles.empty()) {
            return;
        }

        m_nodes.reserve(m_triangles.size() * 2);
        BvhNode root;
        root.leftFirst = 0;
        root.count = static_cast<std::uint32_t>(m_triangles.size());
        m_nodes.push_back(root);
        buildNode(0);
    }

    void MeshCollider::buildNode(std::uint32_t nodeIndex) {
        BvhNode& node = m_nodes[nodeIndex];
        Aabb box;
        Aabb centroidBox;
        for (std::uint32_t i = 0; i < node.count; ++i) {
            const CollisionTriangle& tri = m_triangles[m_triIndices[node.leftFirst + i]];
            box.merge(tri.bounds);
            centroidBox.expand((tri.a + tri.b + tri.c) * (1.0f / 3.0f));
        }
        node.bounds = box;

        if (node.count <= kLeafMaxTriangles) {
            return;
        }

        const glm::vec3 extent = centroidBox.size();
        int splitAxis = 0;
        if (extent.y > extent.x && extent.y >= extent.z) {
            splitAxis = 1;
        } else if (extent.z > extent.x && extent.z > extent.y) {
            splitAxis = 2;
        }
        if (extent[splitAxis] <= kEpsilon) {
            return;
        }

        const float splitPos = centroidBox.center()[splitAxis];
        std::uint32_t i = node.leftFirst;
        std::uint32_t j = node.leftFirst + node.count - 1;
        while (i <= j) {
            const CollisionTriangle& tri = m_triangles[m_triIndices[i]];
            const float centroid = (tri.a[splitAxis] + tri.b[splitAxis] + tri.c[splitAxis]) * (1.0f / 3.0f);
            if (centroid < splitPos) {
                ++i;
            } else {
                std::swap(m_triIndices[i], m_triIndices[j]);
                if (j == 0) {
                    break;
                }
                --j;
            }
        }

        const std::uint32_t leftCount = i - node.leftFirst;
        if (leftCount == 0 || leftCount == node.count) {
            return;
        }

        const std::uint32_t leftChildIdx = static_cast<std::uint32_t>(m_nodes.size());
        const std::uint32_t rightChildIdx = leftChildIdx + 1;
        const std::uint32_t first = node.leftFirst;
        const std::uint32_t totalCount = node.count;

        m_nodes.push_back(BvhNode{Aabb{}, first, leftCount});
        m_nodes.push_back(BvhNode{Aabb{}, first + leftCount, totalCount - leftCount});

        m_nodes[nodeIndex].leftFirst = leftChildIdx;
        m_nodes[nodeIndex].count = 0;

        buildNode(leftChildIdx);
        buildNode(rightChildIdx);
    }

    MeshCollider MeshCollider::fromMesh(const MeshData& mesh, const glm::mat4& transform) {
        MeshCollider collider;
        collider.addMesh(mesh, transform);
        collider.build();
        return collider;
    }

    void MeshCollider::queryAabb(const Aabb& queryBox, std::vector<std::uint32_t>& outIndices) const {
        outIndices.clear();
        if (m_triangles.empty() || queryBox.isEmpty() || !m_bounds.intersects(queryBox)) {
            return;
        }

        // Fallback to linear scan if triangles were added without calling build()
        if (m_nodes.empty()) {
            for (std::size_t i = 0; i < m_triangles.size(); ++i) {
                if (m_triangles[i].bounds.intersects(queryBox)) {
                    outIndices.push_back(static_cast<std::uint32_t>(i));
                }
            }
            return;
        }

        std::array<std::uint32_t, 64> stack{};
        std::size_t stackTop = 0;
        stack[stackTop++] = 0;

        while (stackTop > 0) {
            const BvhNode& node = m_nodes[stack[--stackTop]];
            if (!node.bounds.intersects(queryBox)) {
                continue;
            }
            if (node.count > 0) {
                for (std::uint32_t i = 0; i < node.count; ++i) {
                    const std::uint32_t triIdx = m_triIndices[node.leftFirst + i];
                    if (m_triangles[triIdx].bounds.intersects(queryBox)) {
                        outIndices.push_back(triIdx);
                    }
                }
            } else if (stackTop + 2 <= stack.size()) {
                stack[stackTop++] = node.leftFirst;
                stack[stackTop++] = node.leftFirst + 1;
            }
        }
    }

    std::optional<MeshRaycastHit> MeshCollider::raycast(const Ray& ray, float maxDistance) const {
        if (m_triangles.empty() || maxDistance < 0.0f) {
            return std::nullopt;
        }
        const auto rootHit = intersectRayAabb(ray, m_bounds);
        if (!rootHit || *rootHit > maxDistance) {
            return std::nullopt;
        }

        std::optional<MeshRaycastHit> best;
        float closestT = maxDistance;

        auto testTriangle = [&](std::uint32_t triIdx) {
            const CollisionTriangle& tri = m_triangles[triIdx];
            const auto hitT = intersectRayTriangle(ray, tri.a, tri.b, tri.c);
            if (!hitT || *hitT < 0.0f || *hitT > closestT) {
                return;
            }
            closestT = *hitT;
            MeshRaycastHit hit;
            hit.distance = *hitT;
            hit.point = ray.at(*hitT);
            hit.normal = (glm::dot(tri.normal, ray.direction) <= 0.0f) ? tri.normal : -tri.normal;
            hit.triangleIndex = triIdx;
            best = hit;
        };

        if (m_nodes.empty()) {
            for (std::size_t i = 0; i < m_triangles.size(); ++i) {
                testTriangle(static_cast<std::uint32_t>(i));
            }
            return best;
        }

        std::array<std::uint32_t, 64> stack{};
        std::size_t stackTop = 0;
        stack[stackTop++] = 0;

        while (stackTop > 0) {
            const BvhNode& node = m_nodes[stack[--stackTop]];
            const auto boxT = intersectRayAabb(ray, node.bounds);
            if (!boxT || *boxT > closestT) {
                continue;
            }
            if (node.count > 0) {
                for (std::uint32_t i = 0; i < node.count; ++i) {
                    testTriangle(m_triIndices[node.leftFirst + i]);
                }
            } else if (stackTop + 2 <= stack.size()) {
                const std::uint32_t leftIdx = node.leftFirst;
                const std::uint32_t rightIdx = leftIdx + 1;
                const auto leftT = intersectRayAabb(ray, m_nodes[leftIdx].bounds);
                const auto rightT = intersectRayAabb(ray, m_nodes[rightIdx].bounds);
                const bool hitLeft = leftT && *leftT <= closestT;
                const bool hitRight = rightT && *rightT <= closestT;
                if (hitLeft && hitRight) {
                    // Push farther child first so the nearer child is popped first
                    if (*leftT < *rightT) {
                        stack[stackTop++] = rightIdx;
                        stack[stackTop++] = leftIdx;
                    } else {
                        stack[stackTop++] = leftIdx;
                        stack[stackTop++] = rightIdx;
                    }
                } else if (hitLeft) {
                    stack[stackTop++] = leftIdx;
                } else if (hitRight) {
                    stack[stackTop++] = rightIdx;
                }
            }
        }

        return best;
    }

    ShapeCastHit MeshCollider::sphereCast(const Sphere& sphere, const glm::vec3& delta, float skin) const {
        std::vector<std::uint32_t> candidates;
        return sphereCast(sphere, delta, skin, candidates);
    }

    ShapeCastHit MeshCollider::sphereCast(const Sphere& sphere, const glm::vec3& delta, float skin,
                                          std::vector<std::uint32_t>& candidates) const {
        if (m_triangles.empty() || sphere.radius <= 0.0f) {
            return ShapeCastHit{};
        }
        const float deltaLenSq = glm::dot(delta, delta);
        if (deltaLenSq < 1e-12f) {
            return ShapeCastHit{};
        }
        const glm::vec3 moveDir = delta / std::sqrt(deltaLenSq);

        Aabb sweptBox = Aabb::fromCenterExtents(sphere.center, glm::vec3(sphere.radius));
        sweptBox.merge(Aabb::fromCenterExtents(sphere.center + delta, glm::vec3(sphere.radius)));
        sweptBox.min -= glm::vec3(skin + 0.02f);
        sweptBox.max += glm::vec3(skin + 0.02f);

        queryAabb(sweptBox, candidates);
        if (candidates.empty()) {
            return ShapeCastHit{};
        }

        return sweepShapeImpl(sphere.radius, delta, skin, [&](float t, float testRadius) -> ShapeContact {
            const glm::vec3 pos = sphere.center + delta * t;
            ShapeContact best;
            for (const std::uint32_t triIdx : candidates) {
                const ShapeContact c = contactSphereTriangle(pos, testRadius, m_triangles[triIdx], triIdx);
                if (!c.hit || glm::dot(moveDir, c.normal) >= -1e-5f) {
                    continue;
                }
                const bool deeper = c.penetration > best.penetration + 1e-5f;
                const bool tiedFlatter =
                    std::abs(c.penetration - best.penetration) <= 1e-5f && c.faceNormal.y > best.faceNormal.y;
                if (!best.hit || deeper || tiedFlatter) {
                    best = c;
                }
            }
            return best;
        });
    }

    ShapeCastHit MeshCollider::capsuleCast(const Capsule& capsule, const glm::vec3& delta, float skin) const {
        std::vector<std::uint32_t> candidates;
        return capsuleCast(capsule, delta, skin, candidates);
    }

    ShapeCastHit MeshCollider::capsuleCast(const Capsule& capsule, const glm::vec3& delta, float skin,
                                           std::vector<std::uint32_t>& candidates) const {
        if (m_triangles.empty() || capsule.radius <= 0.0f) {
            return ShapeCastHit{};
        }
        const float deltaLenSq = glm::dot(delta, delta);
        if (deltaLenSq < 1e-12f) {
            return ShapeCastHit{};
        }
        const glm::vec3 moveDir = delta / std::sqrt(deltaLenSq);

        Aabb sweptBox = capsule.bounds();
        Capsule endCap = capsule;
        endCap.feet += delta;
        sweptBox.merge(endCap.bounds());
        sweptBox.min -= glm::vec3(skin + 0.02f);
        sweptBox.max += glm::vec3(skin + 0.02f);

        queryAabb(sweptBox, candidates);
        if (candidates.empty()) {
            return ShapeCastHit{};
        }

        return sweepShapeImpl(capsule.radius, delta, skin, [&](float t, float testRadius) -> ShapeContact {
            Capsule testCap = capsule;
            testCap.feet += delta * t;
            ShapeContact best;
            for (const std::uint32_t triIdx : candidates) {
                const ShapeContact c =
                    contactCapsuleTriangle(testCap, testRadius, m_triangles[triIdx], triIdx);
                if (!c.hit || glm::dot(moveDir, c.normal) >= -1e-5f) {
                    continue;
                }
                const bool deeper = c.penetration > best.penetration + 1e-5f;
                const bool tiedFlatter =
                    std::abs(c.penetration - best.penetration) <= 1e-5f && c.faceNormal.y > best.faceNormal.y;
                if (!best.hit || deeper || tiedFlatter) {
                    best = c;
                }
            }
            return best;
        });
    }

    bool MeshCollider::overlapSphere(const Sphere& sphere, std::vector<ShapeContact>& outContacts,
                                     float extraRadius) const {
        std::vector<std::uint32_t> candidates;
        return overlapSphere(sphere, outContacts, extraRadius, candidates);
    }

    bool MeshCollider::overlapSphere(const Sphere& sphere, std::vector<ShapeContact>& outContacts,
                                     float extraRadius, std::vector<std::uint32_t>& candidates) const {
        outContacts.clear();
        const float testRadius = sphere.radius + extraRadius;
        if (m_triangles.empty() || testRadius <= 0.0f) {
            return false;
        }
        const Aabb queryBox = Aabb::fromCenterExtents(sphere.center, glm::vec3(testRadius + 0.01f));
        queryAabb(queryBox, candidates);
        for (const std::uint32_t triIdx : candidates) {
            const ShapeContact c =
                contactSphereTriangle(sphere.center, testRadius, m_triangles[triIdx], triIdx);
            if (c.hit) {
                outContacts.push_back(c);
            }
        }
        return !outContacts.empty();
    }

    bool MeshCollider::overlapCapsule(const Capsule& capsule, std::vector<ShapeContact>& outContacts,
                                      float extraRadius) const {
        std::vector<std::uint32_t> candidates;
        return overlapCapsule(capsule, outContacts, extraRadius, candidates);
    }

    bool MeshCollider::overlapCapsule(const Capsule& capsule, std::vector<ShapeContact>& outContacts,
                                      float extraRadius, std::vector<std::uint32_t>& candidates) const {
        outContacts.clear();
        const float testRadius = capsule.radius + extraRadius;
        if (m_triangles.empty() || testRadius <= 0.0f) {
            return false;
        }
        Aabb queryBox = capsule.bounds();
        queryBox.min -= glm::vec3(extraRadius + 0.01f);
        queryBox.max += glm::vec3(extraRadius + 0.01f);
        queryAabb(queryBox, candidates);
        for (const std::uint32_t triIdx : candidates) {
            const ShapeContact c = contactCapsuleTriangle(capsule, testRadius, m_triangles[triIdx], triIdx);
            if (c.hit) {
                outContacts.push_back(c);
            }
        }
        return !outContacts.empty();
    }

} // namespace mge
