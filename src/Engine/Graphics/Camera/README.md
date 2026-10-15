# src/Engine/Graphics/Camera/

Namespace: `Engine`. Two **independent** hierarchies:

```
Camera (abstract, int width/height, m_ prefixed members)
└── Camera2D            orthographic, zoom

Camera3D (concrete, not derived from Camera)
└── FirstPersonCamera   walking: gravity, jump, ground snap
```

| File | Purpose | Used by SandboxApp? |
|------|---------|---------------------|
| `Camera.h/.cpp` | Abstract base: position, projection & view matrices | via Camera2D |
| `Camera2D.h/.cpp` | Orthographic camera for 2D/HUD | Yes – `_guiCamera` |
| `Camera3D.h/.cpp` | Perspective free‑fly camera with mouse look + AABB collision callback | Yes – `camera` |
| `FirstPersonCamera.h/.cpp` | Camera3D with gravity, jumping, ground detection | No |

---

## `Camera` (abstract)
- `update()` and `updateProjection(int w, int h)` are pure virtual.
- `get/setPosition(vec3)`, `getProjectionMatrix()`, `getViewMatrix()` (return const refs).

## `Camera2D`
- `updateProjection(w, h)`: `ortho(-aspect*zoom, aspect*zoom, -zoom, zoom, -1, 1)` – a
  **normalised** view (height spans 2 units), *not* pixel coordinates.
- `update()`: `view = translate(-position)`.
- `setZoom/getZoom` – note `setZoom` doesn't rebuild the projection; call `updateProjection` after.
- SandboxApp passes only the projection to `GUILabel::draw` (view is ignored).

## `Camera3D`

| Setting | Default |
|---------|---------|
| FOV | 45° vertical |
| Near / far | 0.1 / 1000 |
| Yaw / Pitch | −90° / 0° (looking down −Z) |
| `MovementSpeed` | 10 units/s (`setSpeed`) |
| `MouseSensitivity` | 0.1 °/pixel (`setSensitivity`) |

`update(float dt)` – returns immediately if `dt ≤ 0.0001` (so time scale 0 = frozen camera):
1. **Mouse look** – only if `InputManager::IsCursorLocked()`. Pitch clamped to ±89°.
2. **Free‑fly movement** – `W/S` along `Front` (includes pitch), `A/D` along `Right`,
   `Space`/`LShift` up/down along world Y. `LCtrl` ×2.5. Direction normalised.
3. **Collision** (if a callback is set) – moves X, then Z, then Y; after each axis, if the AABB
   `Position ± 0.3` overlaps (callback returns true) that axis move is undone. No gravity.
4. `view = lookAt(Position, Position + Front, Up)`.

Other API: `updateProjection(float w, float h)`, `getViewMatrix()`, `getProjectionMatrix()`,
`get/setPosition`, `setCollisionCallback(std::function<bool(const vec3& min, const vec3& max)>)`,
`isSphereVisible(center, radius)` – **stub, always true**.

Members are `protected` and use PascalCase (`Position`, `Front`, `Up`, `Right`, `WorldUp`,
`Yaw`, `Pitch`) plus `_width/_height/_fov/_projectionMatrix/_viewMatrix` and
`m_Velocity/m_IsGrounded/m_CollisionCallback`.

Note: the view matrix is first computed in `update()`; before the first update it is
uninitialised.

## `FirstPersonCamera : Camera3D`

`update(dt)`:
- Mouse look always (ignores cursor lock state).
- `WASD` on the XZ plane (pitch removed), `LCtrl` ×1.5 sprint.
- `Space` while grounded → `velocity.y = 6.5`.
- With collision callback: player AABB is `Position + [(-0.3,-1.5,-0.3) .. (0.3,0.3,0.3)]`
  (eye 1.5 above feet). X then Z resolved like Camera3D; gravity `-18 u/s²`; on downward hit
  snaps feet to the block top (`floor(min.y + 0.5) + 0.5`) and sets grounded; upward hit
  zeroes velocity.
- Without callback: spectator movement (gravity not applied, but jump velocity would persist).

⚠️ It **re‑declares** `m_Velocity`, `m_IsGrounded`, `m_CollisionCallback` and
`setCollisionCallback`, shadowing Camera3D's. Always call `setCollisionCallback` on a
`FirstPersonCamera` typed reference, never through `Camera3D&`.

To switch SandboxApp to walking mode: change the member type `Engine::Camera3D camera;` to
`Engine::FirstPersonCamera camera;` and include its header.
