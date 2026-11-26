#include <mge/physics/CollisionWorld.h>

#include <mge/physics/GridQuery.h>
#include <mge/physics/VoxelCollision.h>
#include <mge/physics/VoxelRaycast.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>

#include <glm/gtc/matrix_transform.hpp>

namespace mge {

    namespace {

        constexpr float kEps = 1e-6f;
        constexpr int kMaxShapeCastSteps = 512;
        constexpr int kShapeCastBisectIters = 10;

        glm::vec3 worldCenterOf(const Collider& col, const glm::vec3& pos, const glm::quat& rot) {
            return pos + rot * col.offset;
        }

        Capsule worldCapsuleOf(const Collider& col, const glm::vec3& pos, const glm::quat& rot) {
            return Capsule{worldCenterOf(col, pos, rot), col.radius, col.height};
        }

        // Closest point on an oriented box (center, rotation, halfExtents) to point p.
        glm::vec3 closestPointOnObb(const glm::vec3& p, const glm::vec3& center, const glm::quat& rot,
                                    const glm::vec3& halfExtents) {
            const glm::quat invRot = glm::conjugate(rot);
            const glm::vec3 local = invRot * (p - center);
            const glm::vec3 clamped = glm::clamp(local, -halfExtents, halfExtents);
            return center + rot * clamped;
        }

        // Closest points between segment [p0, p1] and oriented box.
        glm::vec3 closestPointOnSegmentToPoint(const glm::vec3& p, const glm::vec3& a, const glm::vec3& b) {
            const glm::vec3 ab = b - a;
            const float lenSq = glm::dot(ab, ab);
            if (lenSq <= kEps) return a;
            const float t = std::clamp(glm::dot(p - a, ab) / lenSq, 0.0f, 1.0f);
            return a + ab * t;
        }

        // Closest points between two line segments [p1, q1] and [p2, q2].
        float segmentSegmentDistSq(const glm::vec3& p1, const glm::vec3& q1, const glm::vec3& p2,
                                   const glm::vec3& q2) {
            const glm::vec3 d1 = q1 - p1;
            const glm::vec3 d2 = q2 - p2;
            const glm::vec3 r = p1 - p2;
            const float a = glm::dot(d1, d1);
            const float e = glm::dot(d2, d2);
            const float f = glm::dot(d2, r);
            float s = 0.0f;
            float t = 0.0f;
            if (a <= kEps && e <= kEps) {
                return glm::dot(r, r);
            }
            if (a <= kEps) {
                t = std::clamp(f / e, 0.0f, 1.0f);
            } else {
                const float c = glm::dot(d1, r);
                if (e <= kEps) {
                    s = std::clamp(-c / a, 0.0f, 1.0f);
                } else {
                    const float b = glm::dot(d1, d2);
                    const float denom = a * e - b * b;
                    s = (denom > 1e-12f) ? std::clamp((b * f - c * e) / denom, 0.0f, 1.0f) : 0.0f;
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
            const glm::vec3 c1 = p1 + d1 * s;
            const glm::vec3 c2 = p2 + d2 * t;
            const glm::vec3 diff = c1 - c2;
            return glm::dot(diff, diff);
        }

        // Separating Axis Theorem for two oriented boxes (3 + 3 + 9 axes).
        bool overlapObbObb(const glm::vec3& cA, const glm::quat& rA, const glm::vec3& eA, const glm::vec3& cB,
                           const glm::quat& rB, const glm::vec3& eB) {
            const glm::mat3 mA = glm::mat3_cast(rA);
            const glm::mat3 mB = glm::mat3_cast(rB);
            const glm::vec3 tWorld = cB - cA;
            const glm::vec3 t(glm::dot(tWorld, mA[0]), glm::dot(tWorld, mA[1]), glm::dot(tWorld, mA[2]));

            float rotMat[3][3];
            float absR[3][3];
            for (int i = 0; i < 3; ++i) {
                for (int j = 0; j < 3; ++j) {
                    rotMat[i][j] = glm::dot(mA[i], mB[j]);
                    absR[i][j] = std::abs(rotMat[i][j]) + kEps;
                }
            }

            // Axes of A
            for (int i = 0; i < 3; ++i) {
                const float ra = eA[i];
                const float rb = eB[0] * absR[i][0] + eB[1] * absR[i][1] + eB[2] * absR[i][2];
                if (std::abs(t[i]) > ra + rb) return false;
            }
            // Axes of B
            for (int j = 0; j < 3; ++j) {
                const float ra = eA[0] * absR[0][j] + eA[1] * absR[1][j] + eA[2] * absR[2][j];
                const float rb = eB[j];
                const float proj = std::abs(t[0] * rotMat[0][j] + t[1] * rotMat[1][j] + t[2] * rotMat[2][j]);
                if (proj > ra + rb) return false;
            }
            // Cross-product axes A_i x B_j
            for (int i = 0; i < 3; ++i) {
                const int i1 = (i + 1) % 3;
                const int i2 = (i + 2) % 3;
                for (int j = 0; j < 3; ++j) {
                    const int j1 = (j + 1) % 3;
                    const int j2 = (j + 2) % 3;
                    const float ra = eA[i1] * absR[i2][j] + eA[i2] * absR[i1][j];
                    const float rb = eB[j1] * absR[i][j2] + eB[j2] * absR[i][j1];
                    const float proj = std::abs(t[i2] * rotMat[i1][j] - t[i1] * rotMat[i2][j]);
                    if (proj > ra + rb) return false;
                }
            }
            return true;
        }

        bool overlapSphereObb(const Sphere& s, const glm::vec3& center, const glm::quat& rot,
                              const glm::vec3& halfExtents) {
            const glm::vec3 closest = closestPointOnObb(s.center, center, rot, halfExtents);
            const glm::vec3 d = s.center - closest;
            return glm::dot(d, d) <= s.radius * s.radius;
        }

        // Iterative closest-point check between upright capsule segment and OBB.
        bool overlapCapsuleObb(const Capsule& cap, const glm::vec3& center, const glm::quat& rot,
                               const glm::vec3& halfExtents) {
            const glm::vec3 p0 = cap.bottomCenter();
            const glm::vec3 p1 = cap.topCenter();
            glm::vec3 segPt = 0.5f * (p0 + p1);
            for (int iter = 0; iter < 6; ++iter) {
                const glm::vec3 boxPt = closestPointOnObb(segPt, center, rot, halfExtents);
                segPt = closestPointOnSegmentToPoint(boxPt, p0, p1);
            }
            const glm::vec3 finalBoxPt = closestPointOnObb(segPt, center, rot, halfExtents);
            const glm::vec3 d = segPt - finalBoxPt;
            return glm::dot(d, d) <= cap.radius * cap.radius;
        }

        bool overlapSphereCapsule(const Sphere& s, const Capsule& cap) {
            const glm::vec3 segPt =
                closestPointOnSegmentToPoint(s.center, cap.bottomCenter(), cap.topCenter());
            const glm::vec3 d = s.center - segPt;
            const float sumR = s.radius + cap.radius;
            return glm::dot(d, d) <= sumR * sumR;
        }

        bool overlapCapsuleCapsule(const Capsule& a, const Capsule& b) {
            const float sumR = a.radius + b.radius;
            return segmentSegmentDistSq(a.bottomCenter(), a.topCenter(), b.bottomCenter(), b.topCenter()) <=
                   sumR * sumR;
        }

        // Ray vs oriented box (transform ray into OBB local space, intersect local AABB, compute face
        // normal).
        std::optional<std::pair<float, glm::vec3>> raycastObb(const Ray& ray, const glm::vec3& center,
                                                              const glm::quat& rot,
                                                              const glm::vec3& halfExtents,
                                                              float maxDistance) {
            const glm::quat invRot = glm::conjugate(rot);
            const glm::vec3 localOrigin = invRot * (ray.origin - center);
            const glm::vec3 localDir = invRot * ray.direction;
            const Ray localRay(localOrigin, localDir);
            const Aabb localBox(-halfExtents, halfExtents);
            const auto t = intersectRayAabb(localRay, localBox);
            if (!t || *t < 0.0f || *t > maxDistance) return std::nullopt;

            const glm::vec3 localHit = localRay.at(*t);
            glm::vec3 localNormal(0.0f, 1.0f, 0.0f);
            float minGap = std::numeric_limits<float>::max();
            for (int axis = 0; axis < 3; ++axis) {
                const float gap = std::abs(halfExtents[axis] - std::abs(localHit[axis]));
                if (gap < minGap) {
                    minGap = gap;
                    localNormal = glm::vec3(0.0f);
                    localNormal[axis] = (localHit[axis] >= 0.0f) ? 1.0f : -1.0f;
                }
            }
            return std::make_pair(*t, glm::normalize(rot * localNormal));
        }

        // Ray vs upright capsule (cylinder body + two hemisphere caps).
        std::optional<std::pair<float, glm::vec3>> raycastCapsule(const Ray& ray, const Capsule& cap,
                                                                  float maxDistance) {
            const glm::vec3 a = cap.bottomCenter();
            const glm::vec3 b = cap.topCenter();
            const float r = cap.radius;

            float bestT = maxDistance + 1.0f;
            glm::vec3 bestNormal(0.0f, 1.0f, 0.0f);
            bool hit = false;

            // Endcap spheres
            for (const glm::vec3& c : {a, b}) {
                const auto t = intersectRaySphere(ray, Sphere{c, r});
                if (t && *t >= 0.0f && *t <= maxDistance && *t < bestT) {
                    bestT = *t;
                    const glm::vec3 p = ray.at(*t);
                    const glm::vec3 d = p - c;
                    bestNormal = (glm::dot(d, d) > 1e-12f) ? glm::normalize(d) : -ray.direction;
                    hit = true;
                }
            }

            // Infinite vertical cylinder around (a.x, a.z) clamped to [a.y, b.y]
            const float dx = ray.direction.x;
            const float dz = ray.direction.z;
            const float ox = ray.origin.x - a.x;
            const float oz = ray.origin.z - a.z;
            const float qa = dx * dx + dz * dz;
            if (qa > 1e-12f) {
                const float qb = 2.0f * (ox * dx + oz * dz);
                const float qc = ox * ox + oz * oz - r * r;
                const float disc = qb * qb - 4.0f * qa * qc;
                if (disc >= 0.0f) {
                    const float sqrtDisc = std::sqrt(disc);
                    for (const float tCand :
                         {(-qb - sqrtDisc) / (2.0f * qa), (-qb + sqrtDisc) / (2.0f * qa)}) {
                        if (tCand < 0.0f || tCand > maxDistance || tCand >= bestT) continue;
                        const glm::vec3 p = ray.at(tCand);
                        if (p.y >= a.y && p.y <= b.y) {
                            bestT = tCand;
                            const glm::vec3 radial(p.x - a.x, 0.0f, p.z - a.z);
                            bestNormal =
                                (glm::dot(radial, radial) > 1e-12f) ? glm::normalize(radial) : -ray.direction;
                            hit = true;
                        }
                    }
                }
            }

            if (!hit) return std::nullopt;
            return std::make_pair(bestT, bestNormal);
        }

        // Generic continuous sweep helper against a single analytical shape predicate.
        template <typename OverlapFn, typename NormalFn>
        std::optional<std::pair<float, glm::vec3>> sweepAnalytical(float radius, const glm::vec3& delta,
                                                                   float skin, OverlapFn&& overlapsAt,
                                                                   NormalFn&& normalAt) {
            const float lenSq = glm::dot(delta, delta);
            if (lenSq < 1e-12f) return std::nullopt;
            const float len = std::sqrt(lenSq);
            const glm::vec3 moveDir = delta / len;

            if (overlapsAt(0.0f)) {
                const glm::vec3 n = normalAt(0.0f);
                if (glm::dot(moveDir, n) < -1e-5f) return std::make_pair(0.0f, n);
            }

            const float stepDist = std::max(radius * 0.25f, 0.02f);
            const int steps = std::clamp(static_cast<int>(std::ceil(len / stepDist)), 1, kMaxShapeCastSteps);

            float prevT = 0.0f;
            for (int s = 1; s <= steps; ++s) {
                const float currT = static_cast<float>(s) / static_cast<float>(steps);
                if (!overlapsAt(currT)) {
                    prevT = currT;
                    continue;
                }
                float lo = prevT;
                float hi = currT;
                for (int b = 0; b < kShapeCastBisectIters; ++b) {
                    const float mid = 0.5f * (lo + hi);
                    if (overlapsAt(mid)) {
                        hi = mid;
                    } else {
                        lo = mid;
                    }
                }
                const glm::vec3 n = normalAt(hi);
                const float approach = std::max(-glm::dot(moveDir, n), 0.25f);
                const float backoff = (skin / approach) / len;
                return std::make_pair(std::clamp(hi - backoff, 0.0f, 1.0f), n);
            }
            return std::nullopt;
        }

    } // namespace

    CollisionWorld::CollisionWorld(float broadphaseCellSize) : m_broadphase(broadphaseCellSize) {}

    void CollisionWorld::setVoxelWorld(const VoxelWorld* world, std::uint32_t layer) {
        m_voxelWorld = world;
        m_voxelLayer = layer;
    }

    std::uint64_t CollisionWorld::packEntity(Entity e) {
        return (static_cast<std::uint64_t>(e.index) << 32) | static_cast<std::uint64_t>(e.generation);
    }

    Entity CollisionWorld::unpackEntity(std::uint64_t packed) {
        return Entity{static_cast<std::uint32_t>(packed >> 32),
                      static_cast<std::uint32_t>(packed & 0xFFFFFFFFu)};
    }

    Aabb CollisionWorld::computeWorldBounds(const Collider& col, const glm::vec3& pos, const glm::quat& rot) {
        const glm::vec3 center = worldCenterOf(col, pos, rot);
        switch (col.shape) {
        case ColliderShapeType::Box: {
            const glm::mat3 m = glm::mat3_cast(rot);
            const glm::vec3 absExt(
                std::abs(m[0].x) * col.halfExtents.x + std::abs(m[1].x) * col.halfExtents.y +
                    std::abs(m[2].x) * col.halfExtents.z,
                std::abs(m[0].y) * col.halfExtents.x + std::abs(m[1].y) * col.halfExtents.y +
                    std::abs(m[2].y) * col.halfExtents.z,
                std::abs(m[0].z) * col.halfExtents.x + std::abs(m[1].z) * col.halfExtents.y +
                    std::abs(m[2].z) * col.halfExtents.z);
            return Aabb::fromCenterExtents(center, absExt);
        }
        case ColliderShapeType::Sphere: return Aabb::fromCenterExtents(center, glm::vec3(col.radius));
        case ColliderShapeType::Capsule: return Capsule{center, col.radius, col.height}.bounds();
        case ColliderShapeType::Mesh: return col.mesh ? col.mesh->bounds() : Aabb{};
        }
        return Aabb{};
    }

    bool CollisionWorld::layersInteract(const Collider& a, const Collider& b) {
        return ((a.layer & b.mask) != 0u) && ((b.layer & a.mask) != 0u);
    }

    bool CollisionWorld::entriesOverlap(const Entry& a, const Entry& b) {
        if (!a.worldBounds.intersects(b.worldBounds)) return false;

        const ColliderShapeType sa = a.collider.shape;
        const ColliderShapeType sb = b.collider.shape;

        if (sa == ColliderShapeType::Box && sb == ColliderShapeType::Box) {
            return overlapObbObb(worldCenterOf(a.collider, a.position, a.rotation), a.rotation,
                                 a.collider.halfExtents, worldCenterOf(b.collider, b.position, b.rotation),
                                 b.rotation, b.collider.halfExtents);
        }
        if (sa == ColliderShapeType::Sphere && sb == ColliderShapeType::Sphere) {
            return Sphere{worldCenterOf(a.collider, a.position, a.rotation), a.collider.radius}.intersects(
                Sphere{worldCenterOf(b.collider, b.position, b.rotation), b.collider.radius});
        }
        if (sa == ColliderShapeType::Sphere && sb == ColliderShapeType::Box) {
            return overlapSphereObb(
                Sphere{worldCenterOf(a.collider, a.position, a.rotation), a.collider.radius},
                worldCenterOf(b.collider, b.position, b.rotation), b.rotation, b.collider.halfExtents);
        }
        if (sa == ColliderShapeType::Box && sb == ColliderShapeType::Sphere) {
            return overlapSphereObb(
                Sphere{worldCenterOf(b.collider, b.position, b.rotation), b.collider.radius},
                worldCenterOf(a.collider, a.position, a.rotation), a.rotation, a.collider.halfExtents);
        }
        if (sa == ColliderShapeType::Capsule && sb == ColliderShapeType::Capsule) {
            return overlapCapsuleCapsule(worldCapsuleOf(a.collider, a.position, a.rotation),
                                         worldCapsuleOf(b.collider, b.position, b.rotation));
        }
        if (sa == ColliderShapeType::Sphere && sb == ColliderShapeType::Capsule) {
            return overlapSphereCapsule(
                Sphere{worldCenterOf(a.collider, a.position, a.rotation), a.collider.radius},
                worldCapsuleOf(b.collider, b.position, b.rotation));
        }
        if (sa == ColliderShapeType::Capsule && sb == ColliderShapeType::Sphere) {
            return overlapSphereCapsule(
                Sphere{worldCenterOf(b.collider, b.position, b.rotation), b.collider.radius},
                worldCapsuleOf(a.collider, a.position, a.rotation));
        }
        if (sa == ColliderShapeType::Capsule && sb == ColliderShapeType::Box) {
            return overlapCapsuleObb(worldCapsuleOf(a.collider, a.position, a.rotation),
                                     worldCenterOf(b.collider, b.position, b.rotation), b.rotation,
                                     b.collider.halfExtents);
        }
        if (sa == ColliderShapeType::Box && sb == ColliderShapeType::Capsule) {
            return overlapCapsuleObb(worldCapsuleOf(b.collider, b.position, b.rotation),
                                     worldCenterOf(a.collider, a.position, a.rotation), a.rotation,
                                     a.collider.halfExtents);
        }
        if (sa == ColliderShapeType::Mesh && a.collider.mesh) {
            std::vector<ShapeContact> contacts;
            if (sb == ColliderShapeType::Sphere) {
                return a.collider.mesh->overlapSphere(
                    Sphere{worldCenterOf(b.collider, b.position, b.rotation), b.collider.radius}, contacts);
            }
            if (sb == ColliderShapeType::Capsule) {
                return a.collider.mesh->overlapCapsule(worldCapsuleOf(b.collider, b.position, b.rotation),
                                                       contacts);
            }
        }
        if (sb == ColliderShapeType::Mesh && b.collider.mesh) {
            std::vector<ShapeContact> contacts;
            if (sa == ColliderShapeType::Sphere) {
                return b.collider.mesh->overlapSphere(
                    Sphere{worldCenterOf(a.collider, a.position, a.rotation), a.collider.radius}, contacts);
            }
            if (sa == ColliderShapeType::Capsule) {
                return b.collider.mesh->overlapCapsule(worldCapsuleOf(a.collider, a.position, a.rotation),
                                                       contacts);
            }
        }
        return a.worldBounds.intersects(b.worldBounds);
    }

    void CollisionWorld::upsert(Entity entity, const Collider& collider, const Transform& transform) {
        if (entity.isNull()) return;
        const std::uint64_t key = packEntity(entity);
        Entry entry;
        entry.entity = entity;
        entry.collider = collider;
        entry.position = transform.position;
        entry.rotation = glm::normalize(transform.rotation);
        entry.worldBounds = computeWorldBounds(collider, entry.position, entry.rotation);

        const auto it = m_entityToIndex.find(key);
        if (it != m_entityToIndex.end()) {
            m_entries[it->second] = entry;
        } else {
            m_entityToIndex[key] = m_entries.size();
            m_entries.push_back(entry);
        }
        m_broadphaseDirty = true;
    }

    void CollisionWorld::remove(Entity entity) {
        const std::uint64_t key = packEntity(entity);
        const auto it = m_entityToIndex.find(key);
        if (it == m_entityToIndex.end()) return;

        const std::size_t idx = it->second;
        const std::size_t last = m_entries.size() - 1;
        if (idx != last) {
            m_entries[idx] = m_entries[last];
            m_entityToIndex[packEntity(m_entries[idx].entity)] = idx;
        }
        m_entries.pop_back();
        m_entityToIndex.erase(it);
        m_broadphaseDirty = true;
    }

    void CollisionWorld::clear() {
        m_entries.clear();
        m_entityToIndex.clear();
        m_broadphase.clear();
        m_activeTriggerPairs.clear();
        m_triggerEvents.clear();
        m_broadphaseDirty = false;
    }

    void CollisionWorld::rebuildBroadphase() {
        m_broadphase.clear();
        for (std::size_t i = 0; i < m_entries.size(); ++i) {
            const Entry& e = m_entries[i];
            if (!e.collider.enabled || e.worldBounds.isEmpty()) continue;
            m_broadphase.insert(static_cast<std::uint32_t>(i), e.worldBounds);
        }
        m_broadphaseDirty = false;
    }

    void CollisionWorld::syncFromRegistry(Registry& registry) {
        std::unordered_set<std::uint64_t> seen;
        registry.each<Transform, Collider>([&](Entity e, const Transform& tf, const Collider& col) {
            seen.insert(packEntity(e));
            Transform worldTf = tf;
            if (const auto* wt = registry.tryGet<WorldTransform>(e)) {
                worldTf.position = glm::vec3(wt->matrix[3]);
                glm::mat3 rotMat(wt->matrix);
                for (int colIdx = 0; colIdx < 3; ++colIdx) {
                    const float len = glm::length(rotMat[colIdx]);
                    if (len > kEps) rotMat[colIdx] /= len;
                }
                worldTf.rotation = glm::normalize(glm::quat_cast(rotMat));
            }
            upsert(e, col, worldTf);
        });

        // Remove entries whose entity was destroyed or lost its Collider component
        std::vector<Entity> toRemove;
        for (const Entry& entry : m_entries) {
            if (seen.count(packEntity(entry.entity)) == 0) {
                toRemove.push_back(entry.entity);
            }
        }
        for (const Entity e : toRemove) remove(e);
    }

    const std::vector<TriggerEvent>& CollisionWorld::stepTriggers() {
        if (m_broadphaseDirty) rebuildBroadphase();

        std::vector<std::pair<std::uint32_t, std::uint32_t>> rawPairs;
        m_broadphase.computePairs(rawPairs);

        std::vector<EntityPairKey> currentPairs;
        for (const auto& [idxA, idxB] : rawPairs) {
            const Entry& a = m_entries[idxA];
            const Entry& b = m_entries[idxB];
            if (!a.collider.enabled || !b.collider.enabled) continue;
            if (!a.collider.isTrigger && !b.collider.isTrigger) continue;
            if (!layersInteract(a.collider, b.collider)) continue;
            if (!entriesOverlap(a, b)) continue;

            const std::uint64_t ka = packEntity(a.entity);
            const std::uint64_t kb = packEntity(b.entity);
            currentPairs.push_back(ka < kb ? EntityPairKey{ka, kb} : EntityPairKey{kb, ka});
        }

        std::sort(currentPairs.begin(), currentPairs.end());
        currentPairs.erase(std::unique(currentPairs.begin(), currentPairs.end()), currentPairs.end());

        m_triggerEvents.clear();
        std::size_t i = 0;
        std::size_t j = 0;
        while (i < m_activeTriggerPairs.size() || j < currentPairs.size()) {
            if (j == currentPairs.size() ||
                (i < m_activeTriggerPairs.size() && m_activeTriggerPairs[i] < currentPairs[j])) {
                // Was active, no longer overlapping -> Exit
                const EntityPairKey& p = m_activeTriggerPairs[i++];
                m_triggerEvents.push_back({TriggerEventType::Exit, unpackEntity(p.a), unpackEntity(p.b)});
            } else if (i == m_activeTriggerPairs.size() || currentPairs[j] < m_activeTriggerPairs[i]) {
                // Newly overlapping -> Enter
                const EntityPairKey& p = currentPairs[j++];
                m_triggerEvents.push_back({TriggerEventType::Enter, unpackEntity(p.a), unpackEntity(p.b)});
            } else {
                // Overlapping in both steps -> Stay
                const EntityPairKey& p = currentPairs[j];
                m_triggerEvents.push_back({TriggerEventType::Stay, unpackEntity(p.a), unpackEntity(p.b)});
                ++i;
                ++j;
            }
        }

        m_activeTriggerPairs = std::move(currentPairs);
        return m_triggerEvents;
    }

    std::optional<CollisionRaycastHit> CollisionWorld::raycast(const Ray& ray, float maxDistance,
                                                               std::uint32_t queryMask,
                                                               bool includeTriggers) const {
        if (maxDistance < 0.0f) return std::nullopt;
        std::optional<CollisionRaycastHit> best;
        float closestT = maxDistance;

        for (const Entry& e : m_entries) {
            if (!e.collider.enabled) continue;
            if (!includeTriggers && e.collider.isTrigger) continue;
            if ((e.collider.layer & queryMask) == 0u) continue;
            const auto boxHit = intersectRayAabb(ray, e.worldBounds);
            if (!boxHit || *boxHit > closestT) continue;

            switch (e.collider.shape) {
            case ColliderShapeType::Box: {
                const auto hit = raycastObb(ray, worldCenterOf(e.collider, e.position, e.rotation),
                                            e.rotation, e.collider.halfExtents, closestT);
                if (hit && hit->first <= closestT) {
                    closestT = hit->first;
                    best = CollisionRaycastHit{e.entity, hit->first, ray.at(hit->first), hit->second, false};
                }
                break;
            }
            case ColliderShapeType::Sphere: {
                const glm::vec3 c = worldCenterOf(e.collider, e.position, e.rotation);
                const auto t = intersectRaySphere(ray, Sphere{c, e.collider.radius});
                if (t && *t >= 0.0f && *t <= closestT) {
                    closestT = *t;
                    const glm::vec3 p = ray.at(*t);
                    const glm::vec3 d = p - c;
                    const glm::vec3 n = (glm::dot(d, d) > 1e-12f) ? glm::normalize(d) : -ray.direction;
                    best = CollisionRaycastHit{e.entity, *t, p, n, false};
                }
                break;
            }
            case ColliderShapeType::Capsule: {
                const auto hit =
                    raycastCapsule(ray, worldCapsuleOf(e.collider, e.position, e.rotation), closestT);
                if (hit && hit->first <= closestT) {
                    closestT = hit->first;
                    best = CollisionRaycastHit{e.entity, hit->first, ray.at(hit->first), hit->second, false};
                }
                break;
            }
            case ColliderShapeType::Mesh: {
                if (!e.collider.mesh) break;
                const auto hit = e.collider.mesh->raycast(ray, closestT);
                if (hit && hit->distance <= closestT) {
                    closestT = hit->distance;
                    best = CollisionRaycastHit{e.entity, hit->distance, hit->point, hit->normal, false};
                }
                break;
            }
            }
        }

        if (m_voxelWorld && (m_voxelLayer & queryMask) != 0u) {
            const auto vhit = raycastVoxels(*m_voxelWorld, ray, closestT);
            if (vhit && vhit->distance <= closestT) {
                best = CollisionRaycastHit{kNullEntity, vhit->distance, vhit->point, glm::vec3(vhit->normal),
                                           true};
            }
        }

        return best;
    }

    std::optional<CollisionShapeCastHit> CollisionWorld::sphereCast(const Sphere& sphere,
                                                                    const glm::vec3& delta,
                                                                    std::uint32_t queryMask, float skin,
                                                                    bool includeTriggers) const {
        if (sphere.radius <= 0.0f || glm::dot(delta, delta) < 1e-12f) return std::nullopt;
        Aabb swept = Aabb::fromCenterExtents(sphere.center, glm::vec3(sphere.radius));
        swept.merge(Aabb::fromCenterExtents(sphere.center + delta, glm::vec3(sphere.radius)));
        swept.min -= glm::vec3(skin + 0.02f);
        swept.max += glm::vec3(skin + 0.02f);

        std::optional<CollisionShapeCastHit> best;
        float bestFrac = 1.0f;
        std::vector<std::uint32_t> candidates;

        for (const Entry& e : m_entries) {
            if (!e.collider.enabled) continue;
            if (!includeTriggers && e.collider.isTrigger) continue;
            if ((e.collider.layer & queryMask) == 0u) continue;
            if (!e.worldBounds.intersects(swept)) continue;

            if (e.collider.shape == ColliderShapeType::Mesh && e.collider.mesh) {
                const ShapeCastHit h = e.collider.mesh->sphereCast(sphere, delta, skin, candidates);
                if (h.hit && h.fraction <= bestFrac) {
                    bestFrac = h.fraction;
                    best = CollisionShapeCastHit{e.entity, h.fraction, h.normal, h.point, false};
                }
            } else {
                const auto hit = sweepAnalytical(
                    sphere.radius, delta, skin,
                    [&](float t) {
                        const Sphere s{sphere.center + delta * t, sphere.radius};
                        switch (e.collider.shape) {
                        case ColliderShapeType::Box:
                            return overlapSphereObb(s, worldCenterOf(e.collider, e.position, e.rotation),
                                                    e.rotation, e.collider.halfExtents);
                        case ColliderShapeType::Sphere:
                            return s.intersects(
                                Sphere{worldCenterOf(e.collider, e.position, e.rotation), e.collider.radius});
                        case ColliderShapeType::Capsule:
                            return overlapSphereCapsule(s,
                                                        worldCapsuleOf(e.collider, e.position, e.rotation));
                        case ColliderShapeType::Mesh: return false;
                        }
                        return false;
                    },
                    [&](float t) {
                        const glm::vec3 p = sphere.center + delta * t;
                        glm::vec3 closest = worldCenterOf(e.collider, e.position, e.rotation);
                        if (e.collider.shape == ColliderShapeType::Box) {
                            closest = closestPointOnObb(p, closest, e.rotation, e.collider.halfExtents);
                        } else if (e.collider.shape == ColliderShapeType::Capsule) {
                            const Capsule cap = worldCapsuleOf(e.collider, e.position, e.rotation);
                            closest = closestPointOnSegmentToPoint(p, cap.bottomCenter(), cap.topCenter());
                        }
                        const glm::vec3 d = p - closest;
                        return (glm::dot(d, d) > 1e-12f) ? glm::normalize(d) : -glm::normalize(delta);
                    });
                if (hit && hit->first <= bestFrac) {
                    bestFrac = hit->first;
                    const glm::vec3 hitCenter = sphere.center + delta * hit->first;
                    best = CollisionShapeCastHit{e.entity, hit->first, hit->second,
                                                 hitCenter - hit->second * sphere.radius, false};
                }
            }
        }

        if (m_voxelWorld && (m_voxelLayer & queryMask) != 0u) {
            const Aabb startBox = Aabb::fromCenterExtents(sphere.center, glm::vec3(sphere.radius));
            MoveOptions opt;
            opt.skin = skin;
            const MoveResult mv = moveAabb(startBox, delta, solidQuery(*m_voxelWorld), opt);
            if (mv.blocked.x || mv.blocked.y || mv.blocked.z) {
                const float frac = std::clamp(glm::length(mv.moved) / glm::length(delta), 0.0f, 1.0f);
                if (frac <= bestFrac) {
                    glm::vec3 n(0.0f);
                    if (mv.blocked.x) n.x = (delta.x > 0.0f) ? -1.0f : 1.0f;
                    if (mv.blocked.y) n.y = (delta.y > 0.0f) ? -1.0f : 1.0f;
                    if (mv.blocked.z) n.z = (delta.z > 0.0f) ? -1.0f : 1.0f;
                    n = (glm::dot(n, n) > kEps) ? glm::normalize(n) : -glm::normalize(delta);
                    best = CollisionShapeCastHit{kNullEntity, frac, n, mv.box.center() - n * sphere.radius,
                                                 true};
                }
            }
        }

        return best;
    }

    std::optional<CollisionShapeCastHit> CollisionWorld::capsuleCast(const Capsule& capsule,
                                                                     const glm::vec3& delta,
                                                                     std::uint32_t queryMask, float skin,
                                                                     bool includeTriggers) const {
        if (capsule.radius <= 0.0f || glm::dot(delta, delta) < 1e-12f) return std::nullopt;
        Aabb swept = capsule.bounds();
        Capsule endCap = capsule;
        endCap.feet += delta;
        swept.merge(endCap.bounds());
        swept.min -= glm::vec3(skin + 0.02f);
        swept.max += glm::vec3(skin + 0.02f);

        std::optional<CollisionShapeCastHit> best;
        float bestFrac = 1.0f;
        std::vector<std::uint32_t> candidates;

        for (const Entry& e : m_entries) {
            if (!e.collider.enabled) continue;
            if (!includeTriggers && e.collider.isTrigger) continue;
            if ((e.collider.layer & queryMask) == 0u) continue;
            if (!e.worldBounds.intersects(swept)) continue;

            if (e.collider.shape == ColliderShapeType::Mesh && e.collider.mesh) {
                const ShapeCastHit h = e.collider.mesh->capsuleCast(capsule, delta, skin, candidates);
                if (h.hit && h.fraction <= bestFrac) {
                    bestFrac = h.fraction;
                    best = CollisionShapeCastHit{e.entity, h.fraction, h.normal, h.point, false};
                }
            } else {
                const auto hit = sweepAnalytical(
                    capsule.radius, delta, skin,
                    [&](float t) {
                        Capsule c = capsule;
                        c.feet += delta * t;
                        switch (e.collider.shape) {
                        case ColliderShapeType::Box:
                            return overlapCapsuleObb(c, worldCenterOf(e.collider, e.position, e.rotation),
                                                     e.rotation, e.collider.halfExtents);
                        case ColliderShapeType::Sphere:
                            return overlapSphereCapsule(
                                Sphere{worldCenterOf(e.collider, e.position, e.rotation), e.collider.radius},
                                c);
                        case ColliderShapeType::Capsule:
                            return overlapCapsuleCapsule(c,
                                                         worldCapsuleOf(e.collider, e.position, e.rotation));
                        case ColliderShapeType::Mesh: return false;
                        }
                        return false;
                    },
                    [&](float t) {
                        Capsule c = capsule;
                        c.feet += delta * t;
                        const glm::vec3 mid = 0.5f * (c.bottomCenter() + c.topCenter());
                        glm::vec3 closest = worldCenterOf(e.collider, e.position, e.rotation);
                        if (e.collider.shape == ColliderShapeType::Box) {
                            closest = closestPointOnObb(mid, closest, e.rotation, e.collider.halfExtents);
                        }
                        const glm::vec3 d = mid - closest;
                        return (glm::dot(d, d) > 1e-12f) ? glm::normalize(d) : -glm::normalize(delta);
                    });
                if (hit && hit->first <= bestFrac) {
                    bestFrac = hit->first;
                    const glm::vec3 hitFeet = capsule.feet + delta * hit->first;
                    best = CollisionShapeCastHit{e.entity, hit->first, hit->second, hitFeet, false};
                }
            }
        }

        if (m_voxelWorld && (m_voxelLayer & queryMask) != 0u) {
            MoveOptions opt;
            opt.skin = skin;
            const MoveResult mv = moveAabb(capsule.bounds(), delta, solidQuery(*m_voxelWorld), opt);
            if (mv.blocked.x || mv.blocked.y || mv.blocked.z) {
                const float frac = std::clamp(glm::length(mv.moved) / glm::length(delta), 0.0f, 1.0f);
                if (frac <= bestFrac) {
                    glm::vec3 n(0.0f);
                    if (mv.blocked.x) n.x = (delta.x > 0.0f) ? -1.0f : 1.0f;
                    if (mv.blocked.y) n.y = (delta.y > 0.0f) ? -1.0f : 1.0f;
                    if (mv.blocked.z) n.z = (delta.z > 0.0f) ? -1.0f : 1.0f;
                    n = (glm::dot(n, n) > kEps) ? glm::normalize(n) : -glm::normalize(delta);
                    best = CollisionShapeCastHit{kNullEntity, frac, n, mv.box.center(), true};
                }
            }
        }

        return best;
    }

    void CollisionWorld::overlapAabb(const Aabb& box, std::vector<Entity>& outEntities,
                                     std::uint32_t queryMask, bool includeTriggers) const {
        outEntities.clear();
        if (box.isEmpty()) return;
        for (const Entry& e : m_entries) {
            if (!e.collider.enabled) continue;
            if (!includeTriggers && e.collider.isTrigger) continue;
            if ((e.collider.layer & queryMask) == 0u) continue;
            if (!e.worldBounds.intersects(box)) continue;
            outEntities.push_back(e.entity);
        }
        std::sort(outEntities.begin(), outEntities.end(),
                  [](Entity a, Entity b) { return packEntity(a) < packEntity(b); });
    }

    void CollisionWorld::overlapSphere(const Sphere& sphere, std::vector<Entity>& outEntities,
                                       std::uint32_t queryMask, bool includeTriggers) const {
        outEntities.clear();
        if (sphere.radius <= 0.0f) return;
        const Aabb box = Aabb::fromCenterExtents(sphere.center, glm::vec3(sphere.radius));
        std::vector<ShapeContact> contacts;
        for (const Entry& e : m_entries) {
            if (!e.collider.enabled) continue;
            if (!includeTriggers && e.collider.isTrigger) continue;
            if ((e.collider.layer & queryMask) == 0u) continue;
            if (!e.worldBounds.intersects(box)) continue;

            bool hits = false;
            switch (e.collider.shape) {
            case ColliderShapeType::Box:
                hits = overlapSphereObb(sphere, worldCenterOf(e.collider, e.position, e.rotation), e.rotation,
                                        e.collider.halfExtents);
                break;
            case ColliderShapeType::Sphere:
                hits = sphere.intersects(
                    Sphere{worldCenterOf(e.collider, e.position, e.rotation), e.collider.radius});
                break;
            case ColliderShapeType::Capsule:
                hits = overlapSphereCapsule(sphere, worldCapsuleOf(e.collider, e.position, e.rotation));
                break;
            case ColliderShapeType::Mesh:
                hits = e.collider.mesh && e.collider.mesh->overlapSphere(sphere, contacts);
                break;
            }
            if (hits) outEntities.push_back(e.entity);
        }
        std::sort(outEntities.begin(), outEntities.end(),
                  [](Entity a, Entity b) { return packEntity(a) < packEntity(b); });
    }

    void CollisionWorld::overlapCapsule(const Capsule& capsule, std::vector<Entity>& outEntities,
                                        std::uint32_t queryMask, bool includeTriggers) const {
        outEntities.clear();
        if (capsule.radius <= 0.0f) return;
        const Aabb box = capsule.bounds();
        std::vector<ShapeContact> contacts;
        for (const Entry& e : m_entries) {
            if (!e.collider.enabled) continue;
            if (!includeTriggers && e.collider.isTrigger) continue;
            if ((e.collider.layer & queryMask) == 0u) continue;
            if (!e.worldBounds.intersects(box)) continue;

            bool hits = false;
            switch (e.collider.shape) {
            case ColliderShapeType::Box:
                hits = overlapCapsuleObb(capsule, worldCenterOf(e.collider, e.position, e.rotation),
                                         e.rotation, e.collider.halfExtents);
                break;
            case ColliderShapeType::Sphere:
                hits = overlapSphereCapsule(
                    Sphere{worldCenterOf(e.collider, e.position, e.rotation), e.collider.radius}, capsule);
                break;
            case ColliderShapeType::Capsule:
                hits = overlapCapsuleCapsule(capsule, worldCapsuleOf(e.collider, e.position, e.rotation));
                break;
            case ColliderShapeType::Mesh:
                hits = e.collider.mesh && e.collider.mesh->overlapCapsule(capsule, contacts);
                break;
            }
            if (hits) outEntities.push_back(e.entity);
        }
        std::sort(outEntities.begin(), outEntities.end(),
                  [](Entity a, Entity b) { return packEntity(a) < packEntity(b); });
    }

    void addCollisionWorldSystem(World& world, CollisionWorld& collisionWorld) {
        world.addSystem(SystemPhase::Fixed, "CollisionWorldSystem",
                        [&collisionWorld](World& w, float /*dt*/) {
                            collisionWorld.syncFromRegistry(w.registry());
                            collisionWorld.stepTriggers();
                        });
    }

} // namespace mge
