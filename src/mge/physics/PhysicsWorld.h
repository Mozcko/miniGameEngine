#pragma once

#include <mge/core/ecs/Registry.h>
#include <mge/core/ecs/Transform.h>
#include <mge/core/ecs/World.h>
#include <mge/physics/GridQuery.h>
#include <mge/physics/RigidBody.h>
#include <mge/physics/SpatialHash.h>

#include <glm/glm.hpp>
#include <cstddef>
#include <utility>
#include <vector>

namespace mge {

    struct PhysicsSettings {
        glm::vec3 gravity{0.0f, -28.0f, 0.0f}; // Igual que el personaje
        int solverIterations = 4;
        float positionCorrection = 0.8f; // Fracción de la penetración que se corrige por paso (Baumgarte)
        float penetrationSlop = 0.005f;
        float sleepVelocity = 0.0f; // 0 = sin sleep (opcional)
        float cellSize = 2.0f;      // Del SpatialHash
    };

    class PhysicsWorld {
    public:
        // voxels vacío = sin mundo voxel
        explicit PhysicsWorld(PhysicsSettings initialSettings = {}, SolidQuery initialVoxels = {});

        // Un paso fijo sobre todas las entidades <RigidBody, Transform> del registry
        void step(Registry& registry, float dt);

        PhysicsSettings& settings() { return m_settings; }
        const PhysicsSettings& settings() const { return m_settings; }

        void setVoxels(SolidQuery voxels) { m_voxels = std::move(voxels); }
        const SolidQuery& voxels() const { return m_voxels; }

        // Pares del broadphase en el último paso (para el overlay de rendimiento)
        std::size_t lastPairCount() const { return m_lastPairCount; }

    private:
        struct BodyEntry {
            Entity entity;
            RigidBody* body = nullptr;
            Transform* transform = nullptr;
        };

        PhysicsSettings m_settings;
        SolidQuery m_voxels;
        std::size_t m_lastPairCount = 0;
        SpatialHash m_spatialHash;

        // Buffers miembros para evitar asignaciones de memoria por paso
        std::vector<BodyEntry> m_bodies;
        std::vector<std::pair<std::uint32_t, std::uint32_t>> m_pairs;
    };

    // Registra el sistema en SystemPhase::Fixed; physics debe vivir más que el World
    void addPhysicsSystem(World& world, PhysicsWorld& physics);

} // namespace mge
