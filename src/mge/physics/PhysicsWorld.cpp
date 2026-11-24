#include <mge/physics/PhysicsWorld.h>

#include <mge/core/ecs/Transform.h>
#include <mge/physics/VoxelCollision.h>

#include <algorithm>
#include <cmath>

namespace mge {

    namespace {

        // Minimum approach speed to apply restitution (avoids micro-bounces at rest)
        constexpr float kRestitutionThreshold = 0.5f;

        struct Contact {
            glm::vec3 normal{0.0f, 1.0f, 0.0f}; // Points from A towards B
            float penetration = 0.0f;
            bool hasContact = false;
        };

        // AABB vs AABB collision using the axis of minimum penetration
        Contact collideBoxBox(const glm::vec3& posA, const glm::vec3& extentsA, const glm::vec3& posB,
                              const glm::vec3& extentsB) {
            Contact contact;
            const glm::vec3 d = posB - posA;
            const glm::vec3 totalExtents = extentsA + extentsB;
            const float ovX = totalExtents.x - std::abs(d.x);
            const float ovY = totalExtents.y - std::abs(d.y);
            const float ovZ = totalExtents.z - std::abs(d.z);

            if (ovX <= 0.0f || ovY <= 0.0f || ovZ <= 0.0f) {
                return contact;
            }

            contact.hasContact = true;
            if (d.x == 0.0f && d.y == 0.0f && d.z == 0.0f) {
                // Coincident centers: +Y normal
                contact.normal = glm::vec3(0.0f, 1.0f, 0.0f);
                contact.penetration = std::min({ovX, ovY, ovZ});
                return contact;
            }

            if (ovX < ovY && ovX < ovZ) {
                contact.penetration = ovX;
                contact.normal = glm::vec3(d.x >= 0.0f ? 1.0f : -1.0f, 0.0f, 0.0f);
            } else if (ovY < ovZ) {
                contact.penetration = ovY;
                contact.normal = glm::vec3(0.0f, d.y >= 0.0f ? 1.0f : -1.0f, 0.0f);
            } else {
                contact.penetration = ovZ;
                contact.normal = glm::vec3(0.0f, 0.0f, d.z >= 0.0f ? 1.0f : -1.0f);
            }
            return contact;
        }

        // Sphere vs Sphere collision
        Contact collideSphereSphere(const glm::vec3& posA, float radiusA, const glm::vec3& posB,
                                    float radiusB) {
            Contact contact;
            const glm::vec3 d = posB - posA;
            const float distSq = glm::dot(d, d);
            const float rSum = radiusA + radiusB;

            if (distSq > rSum * rSum) {
                return contact;
            }

            contact.hasContact = true;
            if (distSq <= 1e-12f) {
                // Coincident centers: +Y normal
                contact.normal = glm::vec3(0.0f, 1.0f, 0.0f);
                contact.penetration = rSum;
                return contact;
            }

            const float dist = std::sqrt(distSq);
            contact.normal = d / dist;
            contact.penetration = rSum - dist;
            return contact;
        }

        // Sphere vs Box collision (A = Sphere, B = Box, normal points from A towards B)
        Contact collideSphereBox(const glm::vec3& posSphere, float radiusSphere, const glm::vec3& posBox,
                                 const glm::vec3& extentsBox) {
            Contact contact;
            const glm::vec3 boxMin = posBox - extentsBox;
            const glm::vec3 boxMax = posBox + extentsBox;
            const glm::vec3 clamped = glm::clamp(posSphere, boxMin, boxMax);
            const glm::vec3 diff = clamped - posSphere; // Points from the sphere towards the box
            const float distSq = glm::dot(diff, diff);

            if (distSq > 1e-12f) {
                // Sphere center is outside the box
                if (distSq > radiusSphere * radiusSphere) {
                    return contact;
                }
                const float dist = std::sqrt(distSq);
                contact.hasContact = true;
                contact.normal = diff / dist; // From sphere A towards box B
                contact.penetration = radiusSphere - dist;
                return contact;
            }

            // Sphere center is inside the box: push towards the nearest face
            contact.hasContact = true;
            const glm::vec3 d = posSphere - posBox;
            if (d.x == 0.0f && d.y == 0.0f && d.z == 0.0f) {
                // Coincident centers: +Y normal
                contact.normal = glm::vec3(0.0f, 1.0f, 0.0f);
                contact.penetration = radiusSphere + extentsBox.y;
                return contact;
            }

            const float distLeft = posSphere.x - boxMin.x;
            const float distRight = boxMax.x - posSphere.x;
            const float distBottom = posSphere.y - boxMin.y;
            const float distTop = boxMax.y - posSphere.y;
            const float distBack = posSphere.z - boxMin.z;
            const float distFront = boxMax.z - posSphere.z;

            float minDist = distLeft;
            glm::vec3 normal(1.0f, 0.0f, 0.0f); // From sphere A towards box B

            if (distRight < minDist) {
                minDist = distRight;
                normal = glm::vec3(-1.0f, 0.0f, 0.0f);
            }
            if (distBottom < minDist) {
                minDist = distBottom;
                normal = glm::vec3(0.0f, 1.0f, 0.0f);
            }
            if (distTop < minDist) {
                minDist = distTop;
                normal = glm::vec3(0.0f, -1.0f, 0.0f);
            }
            if (distBack < minDist) {
                minDist = distBack;
                normal = glm::vec3(0.0f, 0.0f, 1.0f);
            }
            if (distFront < minDist) {
                minDist = distFront;
                normal = glm::vec3(0.0f, 0.0f, -1.0f);
            }

            contact.normal = normal;
            contact.penetration = minDist + radiusSphere;
            return contact;
        }

        Contact collideBodies(const RigidBody& bodyA, const Transform& transformA, const RigidBody& bodyB,
                              const Transform& transformB) {
            if (bodyA.shape == ShapeType::Box && bodyB.shape == ShapeType::Box) {
                return collideBoxBox(transformA.position, bodyA.halfExtents, transformB.position,
                                     bodyB.halfExtents);
            }
            if (bodyA.shape == ShapeType::Sphere && bodyB.shape == ShapeType::Sphere) {
                return collideSphereSphere(transformA.position, bodyA.radius, transformB.position,
                                           bodyB.radius);
            }
            if (bodyA.shape == ShapeType::Sphere && bodyB.shape == ShapeType::Box) {
                return collideSphereBox(transformA.position, bodyA.radius, transformB.position,
                                        bodyB.halfExtents);
            }
            Contact contact =
                collideSphereBox(transformB.position, bodyB.radius, transformA.position, bodyA.halfExtents);
            contact.normal = -contact.normal;
            return contact;
        }

        // Cube inscribed in the sphere (half-side = r / sqrt(3)): anti-tunnelling sweep that never blocks
        // prematurely on edges or corners where the actual sphere does not yet touch the block.
        Aabb sphereInnerBounds(const glm::vec3& center, float radius) {
            constexpr float kInvSqrt3 = 0.57735027f;
            const glm::vec3 half(radius * kInvSqrt3);
            return Aabb(center - half, center + half);
        }

        // Sphere vs nearby solid voxels narrowphase (closest point per block, deepest penetration first)
        void resolveSphereVoxels(RigidBody& body, Transform& transform, const SolidQuery& voxels,
                                 const glm::vec3& gravity, float dt, const MoveOptions& moveOpts) {
            constexpr int kMaxSphereVoxelIters = 4;
            constexpr float kFlatFloorNormalY = 0.99f;

            body.grounded = false;
            const float radius = body.radius;

            for (int iter = 0; iter < kMaxSphereVoxelIters; ++iter) {
                const glm::vec3 center = transform.position;
                const int minX = static_cast<int>(std::floor(center.x - radius));
                const int maxX = static_cast<int>(std::floor(center.x + radius));
                const int minY = static_cast<int>(std::floor(center.y - radius));
                const int maxY = static_cast<int>(std::floor(center.y + radius));
                const int minZ = static_cast<int>(std::floor(center.z - radius));
                const int maxZ = static_cast<int>(std::floor(center.z + radius));

                bool found = false;
                float bestPenetration = 0.0f;
                glm::vec3 bestNormal(0.0f, 1.0f, 0.0f); // From block towards sphere

                for (int by = minY; by <= maxY; ++by) {
                    for (int bz = minZ; bz <= maxZ; ++bz) {
                        for (int bx = minX; bx <= maxX; ++bx) {
                            if (!voxels(glm::ivec3(bx, by, bz))) {
                                continue;
                            }
                            const glm::vec3 blockCenter(static_cast<float>(bx) + 0.5f,
                                                        static_cast<float>(by) + 0.5f,
                                                        static_cast<float>(bz) + 0.5f);
                            const Contact c = collideSphereBox(center, radius, blockCenter, glm::vec3(0.5f));
                            if (c.hasContact && c.penetration > bestPenetration) {
                                found = true;
                                bestPenetration = c.penetration;
                                // collideSphereBox points from sphere towards block; invert to push out
                                bestNormal = -c.normal;
                            }
                        }
                    }
                }

                if (!found || bestPenetration <= 0.0f) {
                    break;
                }

                // Push out through moveAabb so the sphere is never pushed into another solid block
                const MoveResult pushRes = moveAabb(sphereInnerBounds(center, radius),
                                                    bestNormal * bestPenetration, voxels, moveOpts);
                transform.position += pushRes.moved;

                const float vn = glm::dot(body.velocity, bestNormal);
                float jn = 0.0f;
                if (vn < 0.0f) {
                    const float e = (-vn >= kRestitutionThreshold) ? body.restitution : 0.0f;
                    jn = -(1.0f + e) * vn;
                    body.velocity += bestNormal * jn;
                }

                // Coulomb friction on flat floors (on sloped edges the sphere rolls off)
                if (bestNormal.y > kFlatFloorNormalY) {
                    if (body.velocity.y <= 0.0f) {
                        body.grounded = true;
                    }
                    if (body.friction > 0.0f) {
                        const float gn = -glm::dot(gravity * body.gravityScale, bestNormal) * dt;
                        const float normalSupport = std::max(jn, std::max(gn, 0.0f));
                        const glm::vec3 vTan =
                            body.velocity - glm::dot(body.velocity, bestNormal) * bestNormal;
                        const float tanSpeedSq = glm::dot(vTan, vTan);
                        if (tanSpeedSq > 1e-12f && normalSupport > 0.0f) {
                            const float tanSpeed = std::sqrt(tanSpeedSq);
                            const float reduction = std::min(body.friction * normalSupport, tanSpeed);
                            body.velocity -= (vTan / tanSpeed) * reduction;
                        }
                    }
                }
            }
        }

    } // namespace

    PhysicsWorld::PhysicsWorld(PhysicsSettings initialSettings, SolidQuery initialVoxels)
        : m_settings(initialSettings), m_voxels(std::move(initialVoxels)),
          m_spatialHash(initialSettings.cellSize) {}

    void PhysicsWorld::step(Registry& registry, float dt) {
        if (dt <= 0.0f) {
            return;
        }

        // 1) Collect bodies into a vector sorted by entity ID (determinism)
        m_bodies.clear();
        registry.view<RigidBody, Transform>().each(
            [this](Entity entity, RigidBody& body, Transform& transform) {
                m_bodies.push_back({entity, &body, &transform});
            });

        std::sort(m_bodies.begin(), m_bodies.end(), [](const BodyEntry& a, const BodyEntry& b) {
            if (a.entity.index != b.entity.index) {
                return a.entity.index < b.entity.index;
            }
            return a.entity.generation < b.entity.generation;
        });

        for (auto& entry : m_bodies) {
            entry.body->previousPosition = entry.transform->position;
        }

        // 2) Integrate velocity for dynamic bodies (semi-implicit Euler)
        for (auto& entry : m_bodies) {
            RigidBody& body = *entry.body;
            if (body.inverseMass() <= 0.0f) {
                body.velocity = glm::vec3(0.0f);
                continue;
            }

            body.velocity += m_settings.gravity * body.gravityScale * dt;
            const float dampingFactor = std::clamp(1.0f - body.linearDamping * dt, 0.0f, 1.0f);
            body.velocity *= dampingFactor;
        }

        // 3) Move dynamic bodies against the voxel world (moveAabb + sphere-voxel narrowphase)
        const MoveOptions moveOpts{0.0f, 1e-4f}; // stepHeight = 0 (rigid bodies do not climb steps)
        for (auto& entry : m_bodies) {
            RigidBody& body = *entry.body;
            Transform& transform = *entry.transform;
            if (body.inverseMass() <= 0.0f) {
                continue;
            }

            if (m_voxels) {
                if (body.shape == ShapeType::Sphere) {
                    // Anti-tunnelling sweep with inscribed cube, then sphere vs solid blocks narrowphase
                    const MoveResult sweepRes = moveAabb(sphereInnerBounds(transform.position, body.radius),
                                                         body.velocity * dt, m_voxels, moveOpts);
                    transform.position += sweepRes.moved;
                    resolveSphereVoxels(body, transform, m_voxels, m_settings.gravity, dt, moveOpts);
                } else {
                    const Aabb currentBounds = body.bounds(transform.position);
                    const MoveResult res = moveAabb(currentBounds, body.velocity * dt, m_voxels, moveOpts);
                    transform.position += res.moved;

                    // On blocked axes, bounce by restitution and zero out micro-bounces if |v| < threshold
                    if (res.blocked.x) {
                        body.velocity.x = -body.velocity.x * body.restitution;
                        if (std::abs(body.velocity.x) < kRestitutionThreshold) {
                            body.velocity.x = 0.0f;
                        }
                    }
                    if (res.blocked.y) {
                        body.velocity.y = -body.velocity.y * body.restitution;
                        if (std::abs(body.velocity.y) < kRestitutionThreshold) {
                            body.velocity.y = 0.0f;
                        }
                    }
                    if (res.blocked.z) {
                        body.velocity.z = -body.velocity.z * body.restitution;
                        if (std::abs(body.velocity.z) < kRestitutionThreshold) {
                            body.velocity.z = 0.0f;
                        }
                    }

                    body.grounded = res.grounded && (body.velocity.y <= 0.0f);

                    // If grounded on the voxel floor, apply horizontal Coulomb friction
                    if (body.grounded) {
                        const float g = std::abs(m_settings.gravity.y) * body.gravityScale;
                        const float frictionDecel = body.friction * g;
                        const float speedReduction = frictionDecel * dt;

                        const float horizSpeed =
                            std::sqrt(body.velocity.x * body.velocity.x + body.velocity.z * body.velocity.z);
                        if (horizSpeed > 0.0f) {
                            if (horizSpeed <= speedReduction) {
                                body.velocity.x = 0.0f;
                                body.velocity.z = 0.0f;
                            } else {
                                const float factor = (horizSpeed - speedReduction) / horizSpeed;
                                body.velocity.x *= factor;
                                body.velocity.z *= factor;
                            }
                        }
                    }
                }
            } else {
                // No voxel world: move freely by velocity
                transform.position += body.velocity * dt;
                body.grounded = false;
            }
        }

        // 4) Broadphase: SpatialHash with all bodies' boxes -> sorted pairs
        if (m_spatialHash.cellSize() != m_settings.cellSize) {
            m_spatialHash = SpatialHash(m_settings.cellSize);
        }
        m_spatialHash.clear();
        for (std::size_t i = 0; i < m_bodies.size(); ++i) {
            const auto& entry = m_bodies[i];
            m_spatialHash.insert(static_cast<std::uint32_t>(i),
                                 entry.body->bounds(entry.transform->position));
        }
        m_spatialHash.computePairs(m_pairs);
        m_lastPairCount = m_pairs.size();

        // 5) Narrowphase + Sequential impulse solver with accumulated impulses
        if (m_pairs.empty() || m_settings.solverIterations <= 0) {
            return;
        }

        m_contacts.clear();

        for (const auto& [idxA, idxB] : m_pairs) {
            BodyEntry& entryA = m_bodies[idxA];
            BodyEntry& entryB = m_bodies[idxB];
            RigidBody& bodyA = *entryA.body;
            RigidBody& bodyB = *entryB.body;
            const Transform& transformA = *entryA.transform;
            const Transform& transformB = *entryB.transform;

            const float invMassA = bodyA.inverseMass();
            const float invMassB = bodyB.inverseMass();
            if (invMassA <= 0.0f && invMassB <= 0.0f) {
                continue; // Both static: immovable
            }

            const Contact contact = collideBodies(bodyA, transformA, bodyB, transformB);
            if (!contact.hasContact || contact.penetration <= 0.0f) {
                continue;
            }

            // Floor support: if a body is grounded (or static) and supports the other vertically,
            // it acts with infinite mass (effInv = 0) so stacks do not sink into the voxel floor
            float effInvA = invMassA;
            float effInvB = invMassB;
            if ((invMassA <= 0.0f || bodyA.grounded) && contact.normal.y > 0.7f) {
                effInvA = 0.0f;
                bodyB.grounded = true;
            } else if ((invMassB <= 0.0f || bodyB.grounded) && contact.normal.y < -0.7f) {
                effInvB = 0.0f;
                bodyA.grounded = true;
            }

            const float effSum = effInvA + effInvB;
            if (effSum <= 0.0f) {
                continue;
            }

            SolverContact sc;
            sc.idxA = idxA;
            sc.idxB = idxB;
            sc.normal = contact.normal;
            sc.effInvA = effInvA;
            sc.effInvB = effInvB;
            sc.effSum = effSum;
            sc.mu = std::sqrt(bodyA.friction * bodyB.friction);

            const float vn0 = glm::dot(bodyB.velocity - bodyA.velocity, contact.normal);
            if (vn0 < -kRestitutionThreshold) {
                const float e = std::min(bodyA.restitution, bodyB.restitution);
                sc.velocityBias = -e * vn0;
            }

            // Gravity contribution for resting contacts (minimum normal impulse for friction)
            if (vn0 <= 1e-3f) {
                const glm::vec3 accA =
                    (effInvA > 0.0f) ? (m_settings.gravity * bodyA.gravityScale) : glm::vec3(0.0f);
                const glm::vec3 accB =
                    (effInvB > 0.0f) ? (m_settings.gravity * bodyB.gravityScale) : glm::vec3(0.0f);
                const float gRel = glm::dot(accB - accA, contact.normal);
                if (gRel < 0.0f) {
                    sc.minNormalImpulse = (-gRel * dt) / effSum;
                }
            }

            m_contacts.push_back(sc);
        }

        // Sequential impulse iterations (accumulated normal and tangential impulses per contact)
        for (int iter = 0; iter < m_settings.solverIterations; ++iter) {
            for (SolverContact& sc : m_contacts) {
                RigidBody& bodyA = *m_bodies[sc.idxA].body;
                RigidBody& bodyB = *m_bodies[sc.idxB].body;

                // Accumulated normal impulse >= 0
                const glm::vec3 vRel = bodyB.velocity - bodyA.velocity;
                const float vn = glm::dot(vRel, sc.normal);
                const float deltaJn = (sc.velocityBias - vn) / sc.effSum;
                const float oldJn = sc.normalImpulse;
                sc.normalImpulse = std::max(oldJn + deltaJn, 0.0f);
                const float appliedJn = sc.normalImpulse - oldJn;
                if (appliedJn != 0.0f) {
                    bodyA.velocity -= sc.normal * (appliedJn * sc.effInvA);
                    bodyB.velocity += sc.normal * (appliedJn * sc.effInvB);
                }

                // Accumulated tangential impulse clamped by |jt| <= mu * max(normalImpulse, minNormalImpulse)
                if (sc.mu > 0.0f) {
                    const glm::vec3 curVRel = bodyB.velocity - bodyA.velocity;
                    const glm::vec3 vTan = curVRel - glm::dot(curVRel, sc.normal) * sc.normal;
                    const glm::vec3 deltaJt = -vTan / sc.effSum;
                    const glm::vec3 oldJt = sc.tangentImpulse;
                    glm::vec3 newJt = oldJt + deltaJt;
                    const float maxFriction = sc.mu * std::max(sc.normalImpulse, sc.minNormalImpulse);
                    const float newJtLenSq = glm::dot(newJt, newJt);
                    if (newJtLenSq > maxFriction * maxFriction) {
                        if (maxFriction > 0.0f && newJtLenSq > 1e-12f) {
                            newJt *= (maxFriction / std::sqrt(newJtLenSq));
                        } else {
                            newJt = glm::vec3(0.0f);
                        }
                    }
                    const glm::vec3 appliedJt = newJt - oldJt;
                    sc.tangentImpulse = newJt;
                    bodyA.velocity -= appliedJt * sc.effInvA;
                    bodyB.velocity += appliedJt * sc.effInvB;
                }
            }
        }

        // Positional correction (Baumgarte) respecting voxels via moveAabb
        for (const SolverContact& sc : m_contacts) {
            RigidBody& bodyA = *m_bodies[sc.idxA].body;
            RigidBody& bodyB = *m_bodies[sc.idxB].body;
            Transform& transformA = *m_bodies[sc.idxA].transform;
            Transform& transformB = *m_bodies[sc.idxB].transform;

            const Contact contact = collideBodies(bodyA, transformA, bodyB, transformB);
            if (!contact.hasContact || contact.penetration <= m_settings.penetrationSlop) {
                continue;
            }

            const float excess = contact.penetration - m_settings.penetrationSlop;
            const float corrMagnitude = excess * m_settings.positionCorrection;
            const glm::vec3 corrVector = contact.normal * (corrMagnitude / sc.effSum);

            if (sc.effInvA > 0.0f) {
                const glm::vec3 corrA = -corrVector * sc.effInvA;
                if (m_voxels) {
                    const Aabb boundsA = bodyA.shape == ShapeType::Sphere
                                             ? sphereInnerBounds(transformA.position, bodyA.radius)
                                             : bodyA.bounds(transformA.position);
                    const MoveResult resA = moveAabb(boundsA, corrA, m_voxels, moveOpts);
                    transformA.position += resA.moved;
                } else {
                    transformA.position += corrA;
                }
            }

            if (sc.effInvB > 0.0f) {
                const glm::vec3 corrB = corrVector * sc.effInvB;
                if (m_voxels) {
                    const Aabb boundsB = bodyB.shape == ShapeType::Sphere
                                             ? sphereInnerBounds(transformB.position, bodyB.radius)
                                             : bodyB.bounds(transformB.position);
                    const MoveResult resB = moveAabb(boundsB, corrB, m_voxels, moveOpts);
                    transformB.position += resB.moved;
                } else {
                    transformB.position += corrB;
                }
            }
        }
    }

    void addPhysicsSystem(World& world, PhysicsWorld& physics) {
        world.addSystem(SystemPhase::Fixed, "PhysicsSystem",
                        [&physics](World& w, float dt) { physics.step(w.registry(), dt); });
    }

} // namespace mge
