#include <mge/physics/CapsuleCharacterController.h>

#include <mge/core/Log.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <limits>
#include <utility>

#include <glm/gtc/quaternion.hpp>

namespace mge {

    namespace {

        constexpr float kEpsilon = 1e-6f;
        constexpr float kMaxDelta = 256.0f;
        constexpr std::uint32_t kLeafMaxTriangles = 4;
        constexpr int kMaxDepenetrationIters = 8;
        constexpr int kMaxSlideIters = 4;
        constexpr int kMaxSlidePlanes = 4;
        constexpr int kSweepBisectIters = 10;
        constexpr int kMaxSweepSteps = 1024;

        bool isFiniteVec3(const glm::vec3& v) {
            return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
        }

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

            // 1. Check direct intersection between segment and triangle (distance = 0)
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

        struct CapsuleTriangleContact {
            bool hit = false;
            glm::vec3 normal{0.0f, 1.0f, 0.0f};     // Points from triangle towards capsule
            glm::vec3 faceNormal{0.0f, 1.0f, 0.0f}; // Triangle geometric normal oriented with `normal`
            glm::vec3 point{0.0f};
            float penetration = 0.0f;
        };

        CapsuleTriangleContact contactCapsuleTriangle(const Capsule& cap, const CollisionTriangle& tri,
                                                      float testRadius) {
            CapsuleTriangleContact contact;
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

        // Depenetrates a capsule from overlapping triangles (deepest penetration first).
        void depenetrateCapsule(Capsule& cap, const MeshCollider& collider, float skin,
                                std::vector<std::uint32_t>& candidates) {
            for (int iter = 0; iter < kMaxDepenetrationIters; ++iter) {
                Aabb queryBox = cap.bounds();
                queryBox.min -= glm::vec3(skin + 0.01f);
                queryBox.max += glm::vec3(skin + 0.01f);
                collider.queryAabb(queryBox, candidates);

                CapsuleTriangleContact deepest;
                for (const std::uint32_t triIdx : candidates) {
                    const CapsuleTriangleContact c =
                        contactCapsuleTriangle(cap, collider.triangle(triIdx), cap.radius);
                    if (c.hit && c.penetration > deepest.penetration) {
                        deepest = c;
                    }
                }

                if (!deepest.hit || deepest.penetration <= 1e-5f) {
                    break;
                }

                cap.feet += deepest.normal * (deepest.penetration + skin);
            }
        }

        struct SweepHit {
            bool hit = false;
            float fraction = 1.0f;
            glm::vec3 normal{0.0f, 1.0f, 0.0f};
            glm::vec3 faceNormal{0.0f, 1.0f, 0.0f};
            glm::vec3 point{0.0f};
        };

        // Continuous swept capsule vs MeshCollider using bounded substeps + bisection refinement.
        SweepHit sweepCapsule(const Capsule& startCap, const glm::vec3& delta, const MeshCollider& collider,
                              float skin, std::vector<std::uint32_t>& candidates) {
            SweepHit result;
            const float deltaLenSq = glm::dot(delta, delta);
            if (deltaLenSq < 1e-12f) {
                return result;
            }

            const float deltaLen = std::sqrt(deltaLenSq);
            const glm::vec3 moveDir = delta / deltaLen;

            Aabb sweptBox = startCap.bounds();
            Capsule endCap = startCap;
            endCap.feet += delta;
            sweptBox.merge(endCap.bounds());
            sweptBox.min -= glm::vec3(skin + 0.02f);
            sweptBox.max += glm::vec3(skin + 0.02f);

            collider.queryAabb(sweptBox, candidates);
            if (candidates.empty()) {
                return result;
            }

            auto findBlockingContact = [&](float t, float testRadius) -> CapsuleTriangleContact {
                Capsule testCap = startCap;
                testCap.feet += delta * t;
                CapsuleTriangleContact best;
                for (const std::uint32_t triIdx : candidates) {
                    const CapsuleTriangleContact c =
                        contactCapsuleTriangle(testCap, collider.triangle(triIdx), testRadius);
                    if (!c.hit || glm::dot(moveDir, c.normal) >= -1e-5f) {
                        continue;
                    }
                    // Prefer deepest penetration; on shared-edge ties prefer flatter face normal
                    const bool deeper = c.penetration > best.penetration + 1e-5f;
                    const bool tiedFlatter =
                        std::abs(c.penetration - best.penetration) <= 1e-5f &&
                        c.faceNormal.y > best.faceNormal.y;
                    if (!best.hit || deeper || tiedFlatter) {
                        best = c;
                    }
                }
                return best;
            };

            // Check if already touching/penetrating a blocking surface at t = 0
            const CapsuleTriangleContact startContact =
                findBlockingContact(0.0f, startCap.radius + skin * 0.5f);
            if (startContact.hit) {
                result.hit = true;
                result.fraction = 0.0f;
                result.normal = startContact.normal;
                result.faceNormal = startContact.faceNormal;
                result.point = startContact.point;
                return result;
            }

            // Sample at intervals <= radius * 0.25 so even a zero-thickness triangle cannot be skipped
            const float maxStepDist = std::max(startCap.radius * 0.25f, 0.02f);
            const int numSteps =
                std::clamp(static_cast<int>(std::ceil(deltaLen / maxStepDist)), 1, kMaxSweepSteps);

            float prevT = 0.0f;
            for (int step = 1; step <= numSteps; ++step) {
                const float currT = static_cast<float>(step) / static_cast<float>(numSteps);
                const CapsuleTriangleContact c = findBlockingContact(currT, startCap.radius);
                if (!c.hit) {
                    prevT = currT;
                    continue;
                }

                // Refine time of impact on [prevT, currT] via bisection
                float lo = prevT;
                float hi = currT;
                CapsuleTriangleContact hitContact = c;
                for (int b = 0; b < kSweepBisectIters; ++b) {
                    const float mid = 0.5f * (lo + hi);
                    const CapsuleTriangleContact midContact = findBlockingContact(mid, startCap.radius);
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
                return result;
            }

            return result;
        }

        // Projects a horizontal displacement onto a walkable slope plane while preserving speed.
        glm::vec3 projectAlongWalkableSlope(const glm::vec3& remaining, const glm::vec3& slopeNormal) {
            const float origLen = glm::length(remaining);
            if (origLen <= kEpsilon || slopeNormal.y <= kEpsilon) {
                return glm::vec3(0.0f);
            }
            const float rise = -(slopeNormal.x * remaining.x + slopeNormal.z * remaining.z) / slopeNormal.y;
            const glm::vec3 tangent(remaining.x, rise, remaining.z);
            const float tanLen = glm::length(tangent);
            if (tanLen <= kEpsilon) {
                return glm::vec3(0.0f);
            }
            return tangent * (origLen / tanLen);
        }

        // Runs horizontal collide-and-slide with walkable slope climbing and steep-slope wall blocking.
        struct HorizontalSlideResult {
            glm::vec3 feet{0.0f};
            glm::vec3 groundNormal{0.0f, 1.0f, 0.0f};
            glm::vec3 steepNormal{0.0f, 1.0f, 0.0f};
            bool touchedWalkable = false;
            bool hitWall = false;
            bool onSteepSlope = false;
            bool hitCeiling = false;
        };

        HorizontalSlideResult slideHorizontal(const Capsule& startCap, const glm::vec3& horizDelta,
                                              const MeshCollider& collider, float minWalkableNormalY,
                                              float skin, std::vector<std::uint32_t>& candidates) {
            HorizontalSlideResult out;
            out.feet = startCap.feet;

            const float origLenSq = glm::dot(horizDelta, horizDelta);
            if (origLenSq < 1e-12f) {
                return out;
            }

            const glm::vec3 origDir = glm::normalize(horizDelta);
            Capsule cap = startCap;
            glm::vec3 remaining = horizDelta;
            std::array<glm::vec3, kMaxSlidePlanes> planes{};
            int planeCount = 0;

            for (int iter = 0; iter < kMaxSlideIters; ++iter) {
                if (glm::dot(remaining, remaining) < 1e-10f) {
                    break;
                }

                const SweepHit hit = sweepCapsule(cap, remaining, collider, skin, candidates);
                cap.feet += remaining * hit.fraction;
                if (!hit.hit) {
                    break;
                }

                remaining *= (1.0f - hit.fraction);
                glm::vec3 slideNormal = hit.normal;
                bool isWalkable = false;

                if (hit.normal.y >= minWalkableNormalY) {
                    isWalkable = true;
                    out.touchedWalkable = true;
                    out.groundNormal = hit.normal;
                } else if (hit.normal.y <= -0.5f) {
                    out.hitCeiling = true;
                } else {
                    out.hitWall = true;
                    if (hit.normal.y > 0.05f) {
                        out.onSteepSlope = true;
                        out.steepNormal = hit.normal;
                    }
                    // Treat steep slopes as vertical walls during horizontal motion so the character
                    // cannot climb slopes steeper than maxSlopeAngle.
                    const glm::vec3 flatNormal(hit.normal.x, 0.0f, hit.normal.z);
                    const float flatLenSq = glm::dot(flatNormal, flatNormal);
                    if (flatLenSq > 1e-8f) {
                        slideNormal = flatNormal / std::sqrt(flatLenSq);
                    }
                }

                // Record unique slide plane
                bool duplicate = false;
                for (int p = 0; p < planeCount; ++p) {
                    if (glm::dot(planes[static_cast<std::size_t>(p)], slideNormal) > 0.999f) {
                        duplicate = true;
                        break;
                    }
                }
                if (!duplicate && planeCount < kMaxSlidePlanes) {
                    planes[static_cast<std::size_t>(planeCount++)] = slideNormal;
                }

                if (planeCount == 1) {
                    if (isWalkable) {
                        remaining = projectAlongWalkableSlope(remaining, slideNormal);
                    } else {
                        remaining -= glm::dot(remaining, slideNormal) * slideNormal;
                    }
                } else if (planeCount == 2) {
                    const glm::vec3 crease = glm::cross(planes[0], planes[1]);
                    const float creaseLenSq = glm::dot(crease, crease);
                    if (creaseLenSq > 1e-8f) {
                        const glm::vec3 creaseDir = crease / std::sqrt(creaseLenSq);
                        remaining = creaseDir * glm::dot(remaining, creaseDir);
                    } else {
                        remaining = glm::vec3(0.0f);
                    }
                } else {
                    remaining = glm::vec3(0.0f);
                }

                // Never slide backwards relative to the original horizontal direction
                if (glm::dot(remaining, origDir) <= 0.0f) {
                    break;
                }
            }

            out.feet = cap.feet;
            return out;
        }

        // Full 3D collide-and-slide (used in Fly mode).
        CapsuleMoveResult moveCapsuleFly(const Capsule& startCap, const glm::vec3& delta,
                                         const MeshCollider& collider, float skin,
                                         std::vector<std::uint32_t>& candidates) {
            CapsuleMoveResult res;
            Capsule cap = startCap;
            depenetrateCapsule(cap, collider, skin, candidates);

            glm::vec3 remaining = delta;
            std::array<glm::vec3, kMaxSlidePlanes> planes{};
            int planeCount = 0;

            for (int iter = 0; iter < kMaxSlideIters; ++iter) {
                if (glm::dot(remaining, remaining) < 1e-10f) {
                    break;
                }
                const SweepHit hit = sweepCapsule(cap, remaining, collider, skin, candidates);
                cap.feet += remaining * hit.fraction;
                if (!hit.hit) {
                    break;
                }

                if (hit.normal.y > 0.5f) {
                    res.grounded = true;
                    res.groundNormal = hit.normal;
                } else if (hit.normal.y < -0.5f) {
                    res.hitCeiling = true;
                } else {
                    res.hitWall = true;
                }

                remaining *= (1.0f - hit.fraction);
                if (planeCount < kMaxSlidePlanes) {
                    planes[static_cast<std::size_t>(planeCount++)] = hit.normal;
                }

                if (planeCount == 1) {
                    remaining -= glm::dot(remaining, hit.normal) * hit.normal;
                } else if (planeCount == 2) {
                    const glm::vec3 crease = glm::cross(planes[0], planes[1]);
                    const float creaseLenSq = glm::dot(crease, crease);
                    if (creaseLenSq > 1e-8f) {
                        const glm::vec3 creaseDir = crease / std::sqrt(creaseLenSq);
                        remaining = creaseDir * glm::dot(remaining, creaseDir);
                    } else {
                        remaining = glm::vec3(0.0f);
                    }
                } else {
                    remaining = glm::vec3(0.0f);
                }
            }

            res.feet = cap.feet;
            res.moved = res.feet - startCap.feet;
            return res;
        }

    } // namespace

    // --- MeshCollider ---

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

    // --- moveCapsule ---

    CapsuleMoveResult moveCapsule(const Capsule& capsule, const glm::vec3& delta,
                                  const MeshCollider& collider, const CapsuleMoveOptions& options) {
        CapsuleMoveResult result;
        result.feet = capsule.feet;

        if (!isFiniteVec3(capsule.feet) || !isFiniteVec3(delta) || !std::isfinite(capsule.radius) ||
            !std::isfinite(capsule.height) || capsule.radius <= 0.0f) {
            return result;
        }

        const float skin = (std::isnan(options.skin) || options.skin < 0.0f) ? 1e-3f : options.skin;

        glm::vec3 clampedDelta = delta;
        for (int axis = 0; axis < 3; ++axis) {
            if (std::abs(clampedDelta[axis]) > kMaxDelta) {
                static std::atomic<bool> warnedClamp{false};
                if (!warnedClamp.exchange(true, std::memory_order_relaxed)) {
                    MGE_LOG_WARN("Physics", "moveCapsule: delta clamped to +-256 units");
                }
                clampedDelta[axis] = std::clamp(clampedDelta[axis], -kMaxDelta, kMaxDelta);
            }
        }

        if (collider.empty()) {
            result.feet = capsule.feet + clampedDelta;
            result.moved = clampedDelta;
            return result;
        }

        std::vector<std::uint32_t> candidates;
        candidates.reserve(32);

        if (options.flyMode) {
            return moveCapsuleFly(capsule, clampedDelta, collider, skin, candidates);
        }

        const float clampedSlopeDeg = std::clamp(options.maxSlopeAngle, 0.0f, 89.0f);
        const float minWalkableNormalY = std::cos(glm::radians(clampedSlopeDeg));

        Capsule cap = capsule;
        depenetrateCapsule(cap, collider, skin, candidates);

        // 1) Horizontal collide-and-slide
        const glm::vec3 horizDelta(clampedDelta.x, 0.0f, clampedDelta.z);
        const HorizontalSlideResult normalHoriz =
            slideHorizontal(cap, horizDelta, collider, minWalkableNormalY, skin, candidates);

        glm::vec3 chosenFeet = normalHoriz.feet;
        bool steppedThisMove = false;
        bool steppedGrounded = false;
        glm::vec3 steppedGroundNormal(0.0f, 1.0f, 0.0f);

        result.hitWall = normalHoriz.hitWall;
        result.hitCeiling = normalHoriz.hitCeiling;
        if (normalHoriz.onSteepSlope) {
            result.onSteepSlope = true;
            result.slideNormal = normalHoriz.steepNormal;
        }

        // 2) Step-up attempt if grounded, moving horizontally, and blocked by an obstacle
        const float stepHeight = std::max(options.stepHeight, 0.0f);
        if (options.allowStepping && options.wasGrounded && stepHeight > 0.0f && clampedDelta.y <= 0.0f &&
            normalHoriz.hitWall && glm::dot(horizDelta, horizDelta) > 1e-10f) {
            // 2a. Sweep up by stepHeight
            const SweepHit upHit =
                sweepCapsule(cap, glm::vec3(0.0f, stepHeight, 0.0f), collider, skin, candidates);
            const float actualUp = stepHeight * upHit.fraction;

            if (actualUp > 1e-3f) {
                Capsule raisedCap = cap;
                raisedCap.feet.y += actualUp;

                // 2b. Horizontal slide from raised position
                const HorizontalSlideResult stepHoriz =
                    slideHorizontal(raisedCap, horizDelta, collider, minWalkableNormalY, skin, candidates);

                // 2c. Sweep down to land on the step surface
                Capsule downCap = raisedCap;
                downCap.feet = stepHoriz.feet;
                const float downDist = actualUp + std::abs(clampedDelta.y) + skin * 2.0f;
                const SweepHit downHit =
                    sweepCapsule(downCap, glm::vec3(0.0f, -downDist, 0.0f), collider, skin, candidates);

                const bool landingWalkable =
                    downHit.hit &&
                    (downHit.normal.y >= minWalkableNormalY || downHit.faceNormal.y >= minWalkableNormalY);

                if (landingWalkable) {
                    const glm::vec3 candidateFeet =
                        downCap.feet + glm::vec3(0.0f, -downDist * downHit.fraction, 0.0f);
                    const float feetRise = candidateFeet.y - cap.feet.y;
                    const float surfaceRise = downHit.point.y - cap.feet.y;

                    const glm::vec2 normalHorizMoved(normalHoriz.feet.x - cap.feet.x,
                                                     normalHoriz.feet.z - cap.feet.z);
                    const glm::vec2 stepHorizMoved(candidateFeet.x - cap.feet.x,
                                                   candidateFeet.z - cap.feet.z);

                    if (feetRise <= stepHeight + 1e-3f && surfaceRise <= stepHeight + 1e-3f &&
                        glm::dot(stepHorizMoved, stepHorizMoved) >
                            glm::dot(normalHorizMoved, normalHorizMoved) + 1e-5f) {
                        chosenFeet = candidateFeet;
                        steppedThisMove = true;
                        steppedGrounded = true;
                        steppedGroundNormal =
                            (downHit.normal.y >= minWalkableNormalY) ? downHit.normal : downHit.faceNormal;
                        result.hitWall = stepHoriz.hitWall;
                        result.onSteepSlope = false;
                    }
                }
            }
        }

        cap.feet = chosenFeet;
        if (steppedThisMove) {
            result.stepped = true;
            result.grounded = steppedGrounded;
            result.groundNormal = steppedGroundNormal;
        }

        // 3) Vertical movement (skipped downward if step-up already landed cleanly on the step)
        if (!steppedThisMove && std::abs(clampedDelta.y) > 1e-9f) {
            glm::vec3 vertRem(0.0f, clampedDelta.y, 0.0f);
            for (int iter = 0; iter < kMaxSlideIters; ++iter) {
                if (glm::dot(vertRem, vertRem) < 1e-10f) {
                    break;
                }
                const SweepHit vertHit = sweepCapsule(cap, vertRem, collider, skin, candidates);
                cap.feet += vertRem * vertHit.fraction;
                if (!vertHit.hit) {
                    break;
                }

                if (clampedDelta.y < 0.0f) {
                    if (vertHit.normal.y >= minWalkableNormalY) {
                        // Landed on walkable ground: stop without sliding downhill
                        result.grounded = true;
                        result.groundNormal = vertHit.normal;
                        result.onSteepSlope = false;
                        break;
                    }
                    // Hit a steep slope while falling: slide down it
                    result.onSteepSlope = true;
                    result.slideNormal = vertHit.normal;
                    vertRem *= (1.0f - vertHit.fraction);
                    vertRem -= glm::dot(vertRem, vertHit.normal) * vertHit.normal;
                } else {
                    // Moving upward: hit ceiling or overhang
                    if (vertHit.normal.y <= -0.1f) {
                        result.hitCeiling = true;
                    }
                    break;
                }
            }
        }

        // 4) Ground probe / ground snapping when walking off downhill slopes or small drops
        if (!result.grounded && clampedDelta.y <= 0.0f) {
            const float probeDist =
                (options.allowGroundSnap && options.wasGrounded && options.groundSnapDistance > 0.0f)
                    ? options.groundSnapDistance
                    : (skin * 2.0f);

            const SweepHit snapHit =
                sweepCapsule(cap, glm::vec3(0.0f, -probeDist, 0.0f), collider, skin, candidates);
            if (snapHit.hit && snapHit.normal.y >= minWalkableNormalY) {
                const float drop = probeDist * snapHit.fraction;
                cap.feet.y -= drop;
                result.grounded = true;
                result.groundNormal = snapHit.normal;
                result.onSteepSlope = false;
                if (options.allowGroundSnap && options.wasGrounded && drop > skin * 2.0f) {
                    result.snapped = true;
                }
            } else if (snapHit.hit && snapHit.normal.y > 0.05f && snapHit.normal.y < minWalkableNormalY) {
                result.onSteepSlope = true;
                result.slideNormal = snapHit.normal;
            }
        }

        result.feet = cap.feet;
        result.moved = result.feet - capsule.feet;
        return result;
    }

    // --- CapsuleCharacterController ---

    void CapsuleCharacterController::look(float deltaYawDegrees, float deltaPitchDegrees) {
        yaw += deltaYawDegrees;
        pitch = std::clamp(pitch + deltaPitchDegrees, -89.0f, 89.0f);
    }

    glm::vec3 CapsuleCharacterController::forward() const {
        const float yawRad = glm::radians(yaw);
        const float pitchRad = glm::radians(pitch);
        return glm::normalize(glm::vec3(std::cos(yawRad) * std::cos(pitchRad), std::sin(pitchRad),
                                        std::sin(yawRad) * std::cos(pitchRad)));
    }

    glm::vec3 CapsuleCharacterController::flatForward() const {
        const float yawRad = glm::radians(yaw);
        return glm::normalize(glm::vec3(std::cos(yawRad), 0.0f, std::sin(yawRad)));
    }

    glm::vec3 CapsuleCharacterController::right() const {
        return glm::normalize(glm::cross(forward(), glm::vec3(0.0f, 1.0f, 0.0f)));
    }

    void CapsuleCharacterController::setMode(CharacterMode newMode) {
        mode = newMode;
        if (mode == CharacterMode::Fly) {
            velocity.y = 0.0f;
        }
    }

    void CapsuleCharacterController::toggleMode() {
        setMode(mode == CharacterMode::Walk ? CharacterMode::Fly : CharacterMode::Walk);
    }

    glm::vec3 stepCapsuleCharacter(CapsuleCharacterController& controller, const glm::vec3& feet, float dt,
                                   const MeshCollider& collider) {
        controller.previousPosition = feet;
        if (dt <= 0.0f || !controller.enabled) {
            return feet;
        }

        if (controller.mode == CharacterMode::Walk) {
            // Desired horizontal velocity on the XZ plane
            glm::vec3 wishDir = controller.flatForward() * controller.input.moveForward +
                                controller.right() * controller.input.moveRight;
            if (glm::dot(wishDir, wishDir) > kEpsilon) {
                wishDir = glm::normalize(wishDir);
            } else {
                wishDir = glm::vec3(0.0f);
            }

            const float speed =
                controller.walkSpeed * (controller.input.sprint ? controller.sprintMultiplier : 1.0f);
            const glm::vec3 wishVel = wishDir * speed;

            // Acceleration towards wishVel (reduced to 1/4 in air)
            const float accelRate =
                controller.grounded ? controller.acceleration : (controller.acceleration * 0.25f);
            const float maxAccel = accelRate * dt;

            glm::vec2 currHoriz(controller.velocity.x, controller.velocity.z);
            const glm::vec2 targetHoriz(wishVel.x, wishVel.z);
            const glm::vec2 diff = targetHoriz - currHoriz;
            const float diffLen = glm::length(diff);
            if (diffLen <= maxAccel || diffLen < kEpsilon) {
                currHoriz = targetHoriz;
            } else {
                currHoriz += (diff / diffLen) * maxAccel;
            }
            controller.velocity.x = currHoriz.x;
            controller.velocity.z = currHoriz.y;

            const bool wasGrounded = controller.grounded;
            bool jumped = false;
            if (controller.grounded && controller.input.jump) {
                controller.velocity.y = controller.jumpSpeed;
                controller.grounded = false;
                jumped = true;
            } else {
                controller.velocity.y -= controller.gravity * dt;
                if (controller.velocity.y < -controller.maxFallSpeed) {
                    controller.velocity.y = -controller.maxFallSpeed;
                }
            }

            CapsuleMoveOptions moveOpts;
            moveOpts.maxSlopeAngle = controller.maxSlopeAngle;
            moveOpts.stepHeight = controller.stepHeight;
            moveOpts.groundSnapDistance = controller.groundSnapDistance;
            moveOpts.skin = controller.skin;
            moveOpts.wasGrounded = wasGrounded && !jumped;
            moveOpts.allowStepping = wasGrounded && !jumped;
            moveOpts.allowGroundSnap = wasGrounded && !jumped;
            moveOpts.flyMode = false;

            const CapsuleMoveResult res =
                moveCapsule(controller.capsule(feet), controller.velocity * dt, collider, moveOpts);

            if (res.grounded) {
                controller.velocity.y = 0.0f;
            } else if (res.hitCeiling && controller.velocity.y > 0.0f) {
                controller.velocity.y = 0.0f;
            } else if (res.onSteepSlope) {
                const float vn = glm::dot(controller.velocity, res.slideNormal);
                if (vn < 0.0f) {
                    controller.velocity -= res.slideNormal * vn;
                }
            }

            controller.grounded = res.grounded;
            controller.groundNormal = res.grounded ? res.groundNormal : glm::vec3(0.0f, 1.0f, 0.0f);
            controller.onSteepSlope = res.onSteepSlope;
            controller.stepped = res.stepped;
            controller.snapped = res.snapped;
            return res.feet;
        }

        // CharacterMode::Fly
        glm::vec3 moveDir = controller.forward() * controller.input.moveForward +
                            controller.right() * controller.input.moveRight +
                            glm::vec3(0.0f, 1.0f, 0.0f) * controller.input.moveUp;
        if (glm::dot(moveDir, moveDir) > kEpsilon) {
            moveDir = glm::normalize(moveDir);
        } else {
            moveDir = glm::vec3(0.0f);
        }

        const float speed =
            controller.flySpeed * (controller.input.sprint ? controller.flySprintMultiplier : 1.0f);
        controller.velocity = moveDir * speed;

        CapsuleMoveOptions moveOpts;
        moveOpts.skin = controller.skin;
        moveOpts.flyMode = true;

        const CapsuleMoveResult res =
            moveCapsule(controller.capsule(feet), controller.velocity * dt, collider, moveOpts);

        if (res.hitCeiling && controller.velocity.y > 0.0f) {
            controller.velocity.y = 0.0f;
        }
        if (res.grounded && controller.velocity.y < 0.0f) {
            controller.velocity.y = 0.0f;
        }
        controller.grounded = res.grounded;
        controller.groundNormal = res.grounded ? res.groundNormal : glm::vec3(0.0f, 1.0f, 0.0f);
        controller.onSteepSlope = false;
        controller.stepped = false;
        controller.snapped = false;
        return res.feet;
    }

    void stepCapsuleCharacter(CapsuleCharacterController& controller, Transform& transform, float dt,
                              const MeshCollider& collider) {
        transform.position = stepCapsuleCharacter(controller, transform.position, dt, collider);
    }

    void addCapsuleCharacterSystem(World& world, const MeshCollider& collider) {
        world.addSystem(
            SystemPhase::Fixed, "CapsuleCharacterSystem", [&collider](World& w, float dt) {
                w.registry().each<CapsuleCharacterController, Transform>(
                    [dt, &collider](Entity /*e*/, CapsuleCharacterController& controller,
                                    Transform& transform) {
                        stepCapsuleCharacter(controller, transform, dt, collider);
                    });
            });
    }

    void addCapsuleCharacterCameraSystem(World& world, const Time& time) {
        world.addSystem(SystemPhase::Render, "CapsuleCharacterCameraSystem", [&time](World& w, float /*dt*/) {
            auto& reg = w.registry();
            reg.each<CapsuleCharacterCamera, Transform>([&reg, &time](Entity /*e*/,
                                                                      const CapsuleCharacterCamera& cam,
                                                                      Transform& camTransform) {
                if (!reg.valid(cam.target)) {
                    return;
                }
                const auto* controller = reg.tryGet<CapsuleCharacterController>(cam.target);
                const auto* targetTransform = reg.tryGet<Transform>(cam.target);
                if (!controller || !targetTransform) {
                    return;
                }
                camTransform.position =
                    capsuleCharacterEyePosition(*controller, *targetTransform, time.alpha());
                camTransform.rotation = glm::quatLookAt(controller->forward(), glm::vec3(0.0f, 1.0f, 0.0f));
            });
        });
    }

} // namespace mge
