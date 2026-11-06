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
