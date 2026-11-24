#include <mge/physics/PhysicsWorld.h>

#include <mge/core/ecs/Transform.h>
#include <mge/physics/VoxelCollision.h>

#include <algorithm>
#include <cmath>

namespace mge {

    namespace {

        struct Contact {
            glm::vec3 normal{0.0f, 1.0f, 0.0f}; // Apunta de A hacia B
            float penetration = 0.0f;
            bool hasContact = false;
        };

        // Colisión AABB vs AABB por eje de mínima penetración
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
                // Centros coincidentes: normal +Y
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

        // Colisión Esfera vs Esfera
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
                // Centros coincidentes: normal +Y
                contact.normal = glm::vec3(0.0f, 1.0f, 0.0f);
                contact.penetration = rSum;
                return contact;
            }

            const float dist = std::sqrt(distSq);
            contact.normal = d / dist;
            contact.penetration = rSum - dist;
            return contact;
        }

        // Colisión Esfera vs Caja (A = Esfera, B = Caja, normal apunta de A hacia B)
        Contact collideSphereBox(const glm::vec3& posSphere, float radiusSphere, const glm::vec3& posBox,
                                 const glm::vec3& extentsBox) {
            Contact contact;
            const glm::vec3 boxMin = posBox - extentsBox;
            const glm::vec3 boxMax = posBox + extentsBox;
            const glm::vec3 clamped = glm::clamp(posSphere, boxMin, boxMax);
            const glm::vec3 diff = clamped - posSphere; // Apunta de la esfera hacia la caja
            const float distSq = glm::dot(diff, diff);

            if (distSq > 1e-12f) {
                // El centro de la esfera está fuera de la caja
                if (distSq > radiusSphere * radiusSphere) {
                    return contact;
                }
                const float dist = std::sqrt(distSq);
                contact.hasContact = true;
                contact.normal = diff / dist; // De esfera A hacia caja B
                contact.penetration = radiusSphere - dist;
                return contact;
            }

            // El centro de la esfera está dentro de la caja: empujar hacia la cara más cercana
            contact.hasContact = true;
            const glm::vec3 d = posSphere - posBox;
            if (d.x == 0.0f && d.y == 0.0f && d.z == 0.0f) {
                // Centros coincidentes: normal +Y
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
            glm::vec3 normal(1.0f, 0.0f, 0.0f); // De esfera A hacia caja B

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

        // Cubo inscrito en la esfera (semilado = r / sqrt(3)): barrido anti-túnel que nunca bloquea
        // prematuramente en aristas o esquinas donde la esfera real todavía no toca el bloque.
        Aabb sphereInnerBounds(const glm::vec3& center, float radius) {
            constexpr float kInvSqrt3 = 0.57735027f;
            const glm::vec3 half(radius * kInvSqrt3);
            return Aabb(center - half, center + half);
        }

        // Narrowphase esfera vs voxeles sólidos cercanos (punto más cercano por bloque, más profundo primero)
        void resolveSphereVoxels(RigidBody& body, Transform& transform, const SolidQuery& voxels,
                                 const glm::vec3& gravity, float dt, const MoveOptions& moveOpts) {
            constexpr int kMaxSphereVoxelIters = 4;
            constexpr float kRestThreshold = 0.5f;
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
                glm::vec3 bestNormal(0.0f, 1.0f, 0.0f); // Del bloque hacia la esfera

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
                                // collideSphereBox apunta de esfera hacia bloque; invertir para empujar fuera
                                bestNormal = -c.normal;
                            }
                        }
                    }
                }

                if (!found || bestPenetration <= 0.0f) {
                    break;
                }

                // Empujar fuera pasando por moveAabb para nunca introducir la esfera en otro bloque sólido
                const MoveResult pushRes = moveAabb(sphereInnerBounds(center, radius),
                                                    bestNormal * bestPenetration, voxels, moveOpts);
                transform.position += pushRes.moved;

                const float vn = glm::dot(body.velocity, bestNormal);
                float jn = 0.0f;
                if (vn < 0.0f) {
                    const float e = (-vn >= kRestThreshold) ? body.restitution : 0.0f;
                    jn = -(1.0f + e) * vn;
                    body.velocity += bestNormal * jn;
                }

                // Fricción de Coulomb sobre suelo plano (en aristas inclinadas la esfera rueda hacia afuera)
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

        // 1) Recolectar cuerpos en un vector ordenado por id de entidad (determinismo)
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

        // 2) Integrar velocidad para cuerpos dinámicos (semi-implicit Euler)
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

        // 3) Mover cuerpos dinámicos contra el mundo voxel (moveAabb + narrowphase esfera-voxel)
        const MoveOptions moveOpts{0.0f, 1e-4f}; // stepHeight = 0 (los cuerpos rígidos no suben escalones)
        for (auto& entry : m_bodies) {
            RigidBody& body = *entry.body;
            Transform& transform = *entry.transform;
            if (body.inverseMass() <= 0.0f) {
                continue;
            }

            if (m_voxels) {
                if (body.shape == ShapeType::Sphere) {
                    // Barrido anti-túnel con el cubo inscrito y luego narrowphase esfera vs bloques sólidos
                    const MoveResult sweepRes = moveAabb(sphereInnerBounds(transform.position, body.radius),
                                                         body.velocity * dt, m_voxels, moveOpts);
                    transform.position += sweepRes.moved;
                    resolveSphereVoxels(body, transform, m_voxels, m_settings.gravity, dt, moveOpts);
                } else {
                    const Aabb currentBounds = body.bounds(transform.position);
                    const MoveResult res = moveAabb(currentBounds, body.velocity * dt, m_voxels, moveOpts);
                    transform.position += res.moved;

                    // En los ejes bloqueados, rebotar según restitución y detener micro-rebotes si |v| < 0.5
                    constexpr float kRestThreshold = 0.5f;

                    if (res.blocked.x) {
                        body.velocity.x = -body.velocity.x * body.restitution;
                        if (std::abs(body.velocity.x) < kRestThreshold) {
                            body.velocity.x = 0.0f;
                        }
                    }
                    if (res.blocked.y) {
                        body.velocity.y = -body.velocity.y * body.restitution;
                        if (std::abs(body.velocity.y) < kRestThreshold) {
                            body.velocity.y = 0.0f;
                        }
                    }
                    if (res.blocked.z) {
                        body.velocity.z = -body.velocity.z * body.restitution;
                        if (std::abs(body.velocity.z) < kRestThreshold) {
                            body.velocity.z = 0.0f;
                        }
                    }

                    body.grounded = res.grounded && (body.velocity.y <= 0.0f);

                    // Si está apoyado en el suelo voxel, aplicar fricción de Coulomb horizontal
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
                // Sin mundo voxel: mover libremente según velocidad
                transform.position += body.velocity * dt;
                body.grounded = false;
            }
        }

        // 4) Broadphase: SpatialHash con las cajas de todos los cuerpos -> pares ordenados
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

        // 5) Narrowphase + Resolución por impulsos secuenciales con impulsos acumulados
        if (m_pairs.empty() || m_settings.solverIterations <= 0) {
            return;
        }

        m_contacts.clear();
        constexpr float kRestitutionThreshold = 0.5f;

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
                continue; // Ambos estáticos: no se mueven
            }

            const Contact contact = collideBodies(bodyA, transformA, bodyB, transformB);
            if (!contact.hasContact || contact.penetration <= 0.0f) {
                continue;
            }

            // Soporte del suelo: si un cuerpo está apoyado en el suelo (o es estático) y soporta al otro
            // verticalmente, actúa con masa infinita (effInv = 0) para no hundirse en el suelo voxel
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

            // Contribución de la gravedad para contactos en reposo (impulso normal mínimo para la fricción)
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

        // Iteraciones de impulsos secuenciales (normal y tangencial acumulados por contacto)
        for (int iter = 0; iter < m_settings.solverIterations; ++iter) {
            for (SolverContact& sc : m_contacts) {
                RigidBody& bodyA = *m_bodies[sc.idxA].body;
                RigidBody& bodyB = *m_bodies[sc.idxB].body;

                // Impulso normal acumulado >= 0
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

                // Impulso tangencial acumulado limitado por |jt| <= mu * max(normalImpulse, minNormalImpulse)
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

        // Corrección posicional (Baumgarte) respetando los voxeles mediante moveAabb
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
