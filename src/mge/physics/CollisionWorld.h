#pragma once

#include <mge/core/ecs/Entity.h>
#include <mge/core/ecs/Registry.h>
#include <mge/core/ecs/Transform.h>
#include <mge/core/ecs/World.h>
#include <mge/core/math/Geometry.h>
#include <mge/physics/MeshCollider.h>
#include <mge/physics/SpatialHash.h>
#include <mge/voxel/VoxelWorld.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

namespace mge {

    // Supported collision shape kinds in CollisionWorld (#203).
    enum class ColliderShapeType : std::uint8_t {
        Box,     // Oriented box (halfExtents, oriented by entity Transform::rotation)
        Sphere,  // Sphere (radius)
        Capsule, // Upright capsule aligned with +Y (radius, height; entity position = feet)
        Mesh     // Static triangle mesh (MeshCollider* with BVH)
    };

    // Motion classification for colliders in CollisionWorld.
    enum class ColliderMotionType : std::uint8_t {
        Static,
        Kinematic
    };

    // ECS component describing a collider or trigger volume attached to an entity with Transform.
    struct Collider {
        ColliderShapeType shape = ColliderShapeType::Box;
        ColliderMotionType motion = ColliderMotionType::Static;
        glm::vec3 offset{0.0f};             // Local-space offset from Transform::position
        glm::vec3 halfExtents{0.5f};        // Box half-extents
        float radius = 0.5f;                // Sphere or Capsule radius
        float height = 1.8f;                // Capsule total height
        const MeshCollider* mesh = nullptr; // Non-owning pointer for ColliderShapeType::Mesh
        std::uint32_t layer = 1u;           // Bitmask of layers this collider belongs to
        std::uint32_t mask = 0xFFFFFFFFu;   // Bitmask of layers this collider interacts with
        bool isTrigger = false;             // True = non-blocking volume that emits trigger events
        bool enabled = true;

        static Collider box(const glm::vec3& halfExtents, bool isTrigger = false) {
            Collider c;
            c.shape = ColliderShapeType::Box;
            c.halfExtents = halfExtents;
            c.isTrigger = isTrigger;
            return c;
        }

        static Collider sphere(float radius, bool isTrigger = false) {
            Collider c;
            c.shape = ColliderShapeType::Sphere;
            c.radius = radius;
            c.isTrigger = isTrigger;
            return c;
        }

        static Collider capsule(float radius, float height, bool isTrigger = false) {
            Collider c;
            c.shape = ColliderShapeType::Capsule;
            c.radius = radius;
            c.height = height;
            c.isTrigger = isTrigger;
            return c;
        }

        static Collider staticMesh(const MeshCollider* mesh) {
            Collider c;
            c.shape = ColliderShapeType::Mesh;
            c.motion = ColliderMotionType::Static;
            c.mesh = mesh;
            return c;
        }
    };

    // Trigger lifecycle event emitted in deterministic (a, b) entity order by CollisionWorld::stepTriggers().
    enum class TriggerEventType : std::uint8_t {
        Enter,
        Stay,
        Exit
    };

    struct TriggerEvent {
        TriggerEventType type = TriggerEventType::Enter;
        Entity a = kNullEntity;
        Entity b = kNullEntity;
    };

    // Result of a raycast query against CollisionWorld (colliders + optional VoxelWorld).
    struct CollisionRaycastHit {
        Entity entity = kNullEntity; // kNullEntity if hitVoxel == true
        float distance = 0.0f;
        glm::vec3 point{0.0f};
        glm::vec3 normal{0.0f, 1.0f, 0.0f};
        bool hitVoxel = false;
    };

    // Result of a continuous sphereCast or capsuleCast against CollisionWorld.
    struct CollisionShapeCastHit {
        Entity entity = kNullEntity; // kNullEntity if hitVoxel == true
        float fraction = 1.0f;
        glm::vec3 normal{0.0f, 1.0f, 0.0f};
        glm::vec3 point{0.0f};
        bool hitVoxel = false;
    };

    // Unified headless collision world combining static/kinematic shape colliders (oriented Box, Sphere,
    // Capsule, MeshCollider), optional VoxelWorld terrain, layer/mask filtering, and deterministic
    // trigger Enter/Stay/Exit events (#203).
    class CollisionWorld {
    public:
        explicit CollisionWorld(float broadphaseCellSize = 4.0f);

        // Optional voxel world treated as an additional static collider source.
        void setVoxelWorld(const VoxelWorld* world, std::uint32_t layer = 1u);
        const VoxelWorld* voxelWorld() const { return m_voxelWorld; }

        // Direct collider management (also driven automatically by syncFromRegistry / ECS system).
        void upsert(Entity entity, const Collider& collider, const Transform& transform);
        void remove(Entity entity);
        void clear();
        std::size_t colliderCount() const { return m_entries.size(); }

        // Syncs all entities with <Transform, Collider> from an ECS Registry into this CollisionWorld.
        // Uses WorldTransform if present, otherwise Transform::localMatrix().
        void syncFromRegistry(Registry& registry);

        // Evaluates overlapping trigger pairs (deterministic order by entity index/generation) and
        // produces Enter / Stay / Exit events since the previous call.
        const std::vector<TriggerEvent>& stepTriggers();
        const std::vector<TriggerEvent>& triggerEvents() const { return m_triggerEvents; }

        // --- Spatial queries ---
        std::optional<CollisionRaycastHit> raycast(const Ray& ray, float maxDistance,
                                                   std::uint32_t queryMask = 0xFFFFFFFFu,
                                                   bool includeTriggers = false) const;

        std::optional<CollisionShapeCastHit> sphereCast(const Sphere& sphere, const glm::vec3& delta,
                                                        std::uint32_t queryMask = 0xFFFFFFFFu,
                                                        float skin = 1e-3f,
                                                        bool includeTriggers = false) const;

        std::optional<CollisionShapeCastHit> capsuleCast(const Capsule& capsule, const glm::vec3& delta,
                                                         std::uint32_t queryMask = 0xFFFFFFFFu,
                                                         float skin = 1e-3f,
                                                         bool includeTriggers = false) const;

        // Overlap queries return matching entities sorted deterministically by (index, generation).
        void overlapAabb(const Aabb& box, std::vector<Entity>& outEntities,
                         std::uint32_t queryMask = 0xFFFFFFFFu, bool includeTriggers = true) const;
        void overlapSphere(const Sphere& sphere, std::vector<Entity>& outEntities,
                           std::uint32_t queryMask = 0xFFFFFFFFu, bool includeTriggers = true) const;
        void overlapCapsule(const Capsule& capsule, std::vector<Entity>& outEntities,
                            std::uint32_t queryMask = 0xFFFFFFFFu, bool includeTriggers = true) const;

    private:
        struct Entry {
            Entity entity = kNullEntity;
            Collider collider;
            glm::vec3 position{0.0f};
            glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
            Aabb worldBounds;
        };

        struct EntityPairKey {
            std::uint64_t a = 0;
            std::uint64_t b = 0;

            bool operator==(const EntityPairKey& o) const { return a == o.a && b == o.b; }
            bool operator<(const EntityPairKey& o) const { return (a != o.a) ? (a < o.a) : (b < o.b); }
        };

        static std::uint64_t packEntity(Entity e);
        static Entity unpackEntity(std::uint64_t packed);
        static Aabb computeWorldBounds(const Collider& col, const glm::vec3& pos, const glm::quat& rot);
        static bool layersInteract(const Collider& a, const Collider& b);
        static bool entriesOverlap(const Entry& a, const Entry& b);

        void rebuildBroadphase();

        SpatialHash m_broadphase;
        std::vector<Entry> m_entries;
        std::unordered_map<std::uint64_t, std::size_t> m_entityToIndex;
        bool m_broadphaseDirty = true;

        const VoxelWorld* m_voxelWorld = nullptr;
        std::uint32_t m_voxelLayer = 1u;

        std::vector<EntityPairKey> m_activeTriggerPairs;
        std::vector<TriggerEvent> m_triggerEvents;
    };

    // Registers a Fixed-phase ECS system that syncs <Transform, Collider> into `collisionWorld`
    // and advances trigger events (Enter/Stay/Exit) every fixed step.
    // `collisionWorld` must outlive `world`.
    void addCollisionWorldSystem(World& world, CollisionWorld& collisionWorld);

} // namespace mge
