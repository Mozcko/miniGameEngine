# src/mge/physics/ → target `mge_physics`

**Physics module (headless)**: swept AABB collision against the voxel world, spatial grid queries,
and character movement resolution. Depends only on `mge_core` and `mge_voxel`, so it is compiled in
every configuration (including `-DMGE_BUILD_MODULES=OFF`). Programs that link it define `MGE_HAS_PHYSICS`,
and `<mge/mge.h>` exposes it automatically.

| File | Contents |
|------|----------|
| `GridQuery.h` | `SolidQuery = function<bool(ivec3)>`, `solidQuery(world)` |
| `VoxelRaycast.h` | Forwarding header for voxel raycasting (`mge::raycastVoxels`) |
| `VoxelCollision.h/.cpp` | `MoveOptions`, `MoveResult`, `moveAabb(box, delta, isSolid/world, options)` |
| `CharacterController.h/.cpp` | `CharacterMode`, `CharacterInput`, `CharacterController`, `CharacterCamera`, `stepCharacter`, ECS systems |
| `SpatialHash.h/.cpp` | `SpatialHash`: 3D uniform-grid broadphase, `computePairs`, `query` |
| `RigidBody.h/.cpp` | `ShapeType` (Box, Sphere), `RigidBody` component |
| `PhysicsWorld.h/.cpp` | `PhysicsSettings`, `PhysicsWorld`, `addPhysicsSystem` |

## Swept AABB Collision – `<mge/physics/VoxelCollision.h>` (#52)

Resolves axis-by-axis continuous collision (order Y, then X, then Z) against solid voxels:

```cpp
mge::MoveOptions options;
options.stepHeight = 0.6f; // Auto-climb up to 0.6 units (disabled if 0)
options.skin = 1e-4f;      // Separation kept from solid walls

mge::MoveResult res = mge::moveAabb(world, entityAabb, velocity * dt, options);
entityAabb = res.box;
if (res.grounded) {
    velocityY = 0.0f;
}
```

- **Anti-tunnelling**: scans every candidate block across the full swept volume on each axis.
- **Sliding**: resolves Y, then X, then Z independently; hitting a wall along X preserves movement along Z and Y.
- **Stuck tolerance**: blocks that already overlap the box at the start of an axis move are ignored, allowing entities stuck inside terrain to walk out.
- **Step-up**: when a grounded entity's horizontal move is blocked, it attempts `up(stepHeight) -> X -> Z -> down(stepHeight + |delta.y|)`. The stepped trajectory is kept only if horizontal progress improves.
- **Chunk-agnostic**: operates across chunk boundaries in world space; unloaded chunks read as air.

## Character Controller – `<mge/physics/CharacterController.h>` (#53)

Decouples player movement and physics simulation from the camera:

- **Entity Transform convention**: `Transform::position` represents the entity's **feet** (center of the base of the collision bounding box).
- **Bounding box**: `characterAabb(feet, halfExtents)` computes `min = [feet.x - extents.x, feet.y, feet.z - extents.z]`, `max = [feet.x + extents.x, feet.y + 2 * extents.y, feet.z + extents.z]`.
- **Modes**:
  - `CharacterMode::Walk`: planar acceleration towards target horizontal velocity (`flatForward * moveForward + right * moveRight`), gravity (default 28 m/s²), jump (only when grounded), and auto step-up. Air acceleration is reduced (1/4 of ground acceleration).
  - `CharacterMode::Fly`: 3D velocity (`forward * moveForward + right * moveRight + up * moveUp`), no gravity, same collision checks.
- **Fixed-step integration**: `stepCharacter(controller, feet, dt, isSolid)` is a pure, headless function that steps velocity, gravity, and swept AABB collision.
- **ECS Integration**:
  - `addCharacterSystem(world, isSolid)`: runs in `SystemPhase::Fixed`, applies `stepCharacter` to all entities with `Transform` and `CharacterController` when `controller.enabled` is true.
  - `CharacterCamera`: component placed on a camera entity referencing a player entity.
  - `addCharacterCameraSystem(world, time)`: runs in `SystemPhase::Render`, interpolates camera position between `previousPosition` and `currentPosition` using `time.alpha()` and adds `eyeHeight`, aligning camera rotation to `controller.forward()`.

## Spatial-Hash Broadphase – `<mge/physics/SpatialHash.h>` (#54)

Uniform 3D spatial grid broadphase for fast overlap detection and bounding box queries:

- **Rebuilt every step**: `clear()` preserves allocated memory (vector capacities) to eliminate allocations in steady state; entities reinsert each frame via `insert(id, box)`.
- **Touching and overlapping pairs**: `computePairs(outPairs)` finds all unique pairs `(a, b)` with `a < b` whose AABBs touch or overlap (`Aabb::intersects`). Results are sorted lexicographically `(a, b)` and deduplicated across cells.
- **Volume queries**: `query(box, outIds)` returns all inserted IDs whose AABB touches or overlaps `box`, sorted ascending and deduplicated.
- **Large box support**: Boxes spanning multiple cells (e.g. > 8 per axis) insert into all overlapping cells ($O(dx \cdot dy \cdot dz)$). Performance scales with box volume, but all overlap configurations are handled correctly without bounds restrictions.

## Basic Rigid Bodies – `<mge/physics/RigidBody.h>`, `<mge/physics/PhysicsWorld.h>` (#54)

Basic rigid bodies without rotation, simulated against the voxel world and other bodies:

- **Entity Transform convention**: `Transform::position` represents the **center** of the body.
- **Shapes**:
  - `ShapeType::Box`: axis-aligned box with `halfExtents`.
  - `ShapeType::Sphere`: sphere with `radius`.
- **Properties**: `mass` (0 = static / infinite mass), `restitution` (rebound factor $0..1$), `friction` (Coulomb friction $0..1$), `gravityScale`, `linearDamping`, `velocity`, `previousPosition` (for render interpolation via `time.alpha()`), and `grounded`.
- **Simulation Pipeline (`PhysicsWorld::step`)**:
  1. **Deterministic collection**: collects all `<RigidBody, Transform>` entities sorted by entity ID (`index`, then `generation`), storing `previousPosition = position`.
  2. **Velocity integration**: semi-implicit Euler integration for dynamic bodies (`inverseMass() > 0`) using `settings.gravity * gravityScale` and `linearDamping`.
  3. **Voxel world collision**: dynamic bodies sweep against voxels using `moveAabb(bounds, velocity * dt)`. Bounces on blocked axes according to `restitution` (micro-bounces below 0.5 m/s zero out); grounded bodies experience horizontal Coulomb friction deceleration.
  4. **Broadphase**: `SpatialHash` rebuilt with all bodies' bounds finds sorted, deduplicated collision pairs (`lastPairCount()`).
  5. **Narrowphase & Iterative Solver**:
     - Contact detection for Box–Box (minimum penetration axis), Sphere–Sphere, and Sphere–Box (closest point).
     - Coincident centers resolve along $+Y$ to prevent division by zero.
     - Stack stability & floor support: if a body is grounded on voxels or static and supports another vertically ($n_y > 0.7$), it acts with infinite effective mass so dynamic stacks remain stable on the floor without sinking.
     - Baumgarte positional correction split by inverse mass: $\max(\text{penetration} - \text{slop}, 0) \cdot (\text{positionCorrection} / \text{solverIterations})$.
     - Restitution impulse along normal with $e = \min(e_A, e_B)$ (clamped to 0 for relative velocity $< 0.5$ m/s).
     - Tangential friction impulse clamped to the Coulomb friction cone $\mu \cdot j_n$ ($\mu = \sqrt{\mu_A \cdot \mu_B}$).
- **ECS Integration**: `addPhysicsSystem(world, physics)` registers the physics step in `SystemPhase::Fixed`.



