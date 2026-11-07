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

        // 3) Mover cuerpos dinámicos contra el mundo voxel (moveAabb)
        const MoveOptions moveOpts{0.0f, 1e-4f}; // stepHeight = 0 (los cuerpos rígidos no suben escalones)
        for (auto& entry : m_bodies) {
            RigidBody& body = *entry.body;
            Transform& transform = *entry.transform;
            if (body.inverseMass() <= 0.0f) {
                continue;
            }

            if (m_voxels) {
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

        // 5) Narrowphase + Resolución de contactos: repetir solverIterations veces
        if (m_pairs.empty() || m_settings.solverIterations <= 0) {
            return;
        }

        const float posCorrFraction =
            m_settings.positionCorrection / static_cast<float>(m_settings.solverIterations);

        for (int iter = 0; iter < m_settings.solverIterations; ++iter) {
            for (const auto& [idxA, idxB] : m_pairs) {
                BodyEntry& entryA = m_bodies[idxA];
                BodyEntry& entryB = m_bodies[idxB];
                RigidBody& bodyA = *entryA.body;
                RigidBody& bodyB = *entryB.body;
                Transform& transformA = *entryA.transform;
                Transform& transformB = *entryB.transform;

                const float invMassA = bodyA.inverseMass();
                const float invMassB = bodyB.inverseMass();
                if (invMassA <= 0.0f && invMassB <= 0.0f) {
                    continue; // Ambos estáticos: no se mueven
                }

                Contact contact;
                if (bodyA.shape == ShapeType::Box && bodyB.shape == ShapeType::Box) {
                    contact = collideBoxBox(transformA.position, bodyA.halfExtents, transformB.position,
                                            bodyB.halfExtents);
                } else if (bodyA.shape == ShapeType::Sphere && bodyB.shape == ShapeType::Sphere) {
                    contact = collideSphereSphere(transformA.position, bodyA.radius, transformB.position,
                                                  bodyB.radius);
                } else if (bodyA.shape == ShapeType::Sphere && bodyB.shape == ShapeType::Box) {
                    contact = collideSphereBox(transformA.position, bodyA.radius, transformB.position,
                                               bodyB.halfExtents);
                } else { // bodyA.shape == ShapeType::Box && bodyB.shape == ShapeType::Sphere
                    contact = collideSphereBox(transformB.position, bodyB.radius, transformA.position,
                                               bodyA.halfExtents);
                    contact.normal = -contact.normal;
                }

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

                // Corrección posicional (Baumgarte)
                const float excess = std::max(contact.penetration - m_settings.penetrationSlop, 0.0f);
                const float corrMagnitude = excess * posCorrFraction;
                const glm::vec3 corrVector = contact.normal * (corrMagnitude / effSum);

                if (effInvA > 0.0f) {
                    const glm::vec3 corrA = -corrVector * effInvA;
                    if (m_voxels) {
                        const MoveResult resA =
                            moveAabb(bodyA.bounds(transformA.position), corrA, m_voxels, {0.0f, 1e-4f});
                        transformA.position += resA.moved;
                    } else {
                        transformA.position += corrA;
                    }
                }

                if (effInvB > 0.0f) {
                    const glm::vec3 corrB = corrVector * effInvB;
                    if (m_voxels) {
                        const MoveResult resB =
                            moveAabb(bodyB.bounds(transformB.position), corrB, m_voxels, {0.0f, 1e-4f});
                        transformB.position += resB.moved;
                    } else {
                        transformB.position += corrB;
                    }
                }

                // Impulso de velocidad normal
                const glm::vec3 vRel = bodyB.velocity - bodyA.velocity;
                const float vn = glm::dot(vRel, contact.normal);
                float jn = 0.0f;

                if (vn < 0.0f) {
                    float e = std::min(bodyA.restitution, bodyB.restitution);
                    if (-vn < 0.5f) {
                        e = 0.0f; // Detener micro-rebotes
                    }
                    jn = -(1.0f + e) * vn / effSum;
                    bodyA.velocity -= contact.normal * (jn * effInvA);
                    bodyB.velocity += contact.normal * (jn * effInvB);
                }

                // Impulso de fricción tangencial (Coulomb)
                if (jn > 0.0f) {
                    const glm::vec3 currentVRel = bodyB.velocity - bodyA.velocity;
                    const glm::vec3 vTangent =
                        currentVRel - glm::dot(currentVRel, contact.normal) * contact.normal;
                    const float tanSpeedSq = glm::dot(vTangent, vTangent);
                    if (tanSpeedSq > 1e-10f) {
                        const float tanSpeed = std::sqrt(tanSpeedSq);
                        const glm::vec3 t = vTangent / tanSpeed;
                        const float mu = std::sqrt(bodyA.friction * bodyB.friction);
                        const float maxFriction = mu * jn;
                        const float maxJt = tanSpeed / effSum;
                        const float jt = -std::min(maxFriction, maxJt);
                        bodyA.velocity -= t * (jt * effInvA);
                        bodyB.velocity += t * (jt * effInvB);
                    }
                }
            }
        }
    }

    void addPhysicsSystem(World& world, PhysicsWorld& physics) {
        world.addSystem(SystemPhase::Fixed, "PhysicsSystem",
                        [&physics](World& w, float dt) { physics.step(w.registry(), dt); });
    }

} // namespace mge
