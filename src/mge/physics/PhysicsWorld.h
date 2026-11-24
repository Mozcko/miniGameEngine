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
        glm::vec3 gravity{0.0f, -28.0f, 0.0f}; // Same as character controller
        int solverIterations = 4;
        float positionCorrection = 0.8f; // Fraction of penetration corrected per step (Baumgarte)
        float penetrationSlop = 0.005f;
        float sleepVelocity = 0.0f; // 0 = no sleep (optional)
        float cellSize = 2.0f;      // For SpatialHash
    };

    class PhysicsWorld {
    public:
        // Empty voxels = no voxel world
        explicit PhysicsWorld(PhysicsSettings initialSettings = {}, SolidQuery initialVoxels = {});

        // One fixed step over all <RigidBody, Transform> entities in the registry
        void step(Registry& registry, float dt);

        PhysicsSettings& settings() { return m_settings; }
        const PhysicsSettings& settings() const { return m_settings; }

        void setVoxels(SolidQuery voxels) { m_voxels = std::move(voxels); }
        const SolidQuery& voxels() const { return m_voxels; }

        // Broadphase pairs from the last step (for the performance overlay)
        std::size_t lastPairCount() const { return m_lastPairCount; }

    private:
        struct BodyEntry {
            Entity entity;
            RigidBody* body = nullptr;
            Transform* transform = nullptr;
        };

        struct SolverContact {
            std::uint32_t idxA = 0;
            std::uint32_t idxB = 0;
            glm::vec3 normal{0.0f, 1.0f, 0.0f};
            float effInvA = 0.0f;
            float effInvB = 0.0f;
            float effSum = 0.0f;
            float velocityBias = 0.0f;
            float mu = 0.0f;
            float minNormalImpulse = 0.0f;
            float normalImpulse = 0.0f;
            glm::vec3 tangentImpulse{0.0f};
        };

        PhysicsSettings m_settings;
        SolidQuery m_voxels;
        std::size_t m_lastPairCount = 0;
        SpatialHash m_spatialHash;

        // Member buffers to avoid memory allocations per step
        std::vector<BodyEntry> m_bodies;
        std::vector<std::pair<std::uint32_t, std::uint32_t>> m_pairs;
        std::vector<SolverContact> m_contacts;
    };

    // Registers the system in SystemPhase::Fixed; physics must outlive the World
    void addPhysicsSystem(World& world, PhysicsWorld& physics);

} // namespace mge
