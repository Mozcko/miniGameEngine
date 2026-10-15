# src/mge/render/camera/

Namespace: `mge`. Cameras don't read the keyboard: the game converts its actions into a
`CameraInput` { `moveForward`, `moveRight`, `moveUp` (−1..1), `sprint`, `jump`, `lookX`, `lookY`
(mouse pixels) }. So `mge_render` doesn't depend on the platform (#15).

Two **independent** hierarchies:

```
Camera (abstract, int width/height, m_ prefixed members)
└── Camera2D            orthographic, zoom

Camera3D (concrete, not derived from Camera)
└── FirstPersonCamera   walking: gravity, jump, ground snap
```

| File | Purpose | Used by SandboxApp? |
|------|---------|---------------------|
| `Camera.h/.cpp` | Abstract base: position, projection & view matrices | via Camera2D |
| `Camera2D.h/.cpp` | Orthographic camera for 2D | No (the HUD was removed in #9) |
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

## `Camera3D`

| Setting | Default |
|---------|---------|
| FOV | 45° vertical |
| Near / far | 0.1 / 1000 |
| Yaw / Pitch | −90° / 0° (looking down −Z) |
| `MovementSpeed` | 10 units/s (`setSpeed`) |
| `MouseSensitivity` | 0.1 °/pixel (`setSensitivity`) |

`update(float dt, const CameraInput& input)` – returns immediately if `dt ≤ 0.0001` (so time scale 0 = frozen camera):
1. **Mouse look** from `input.lookX/lookY` (the game passes 0 when the cursor isn't captured). Pitch clamped to ±89°.
2. **Free‑fly movement** – `moveForward` along `Front` (includes pitch), `moveRight` along `Right`,
   `moveUp` along world Y; `sprint` ×2.5. Direction normalised.
3. **Collision** (if a callback is set) – moves X, then Z, then Y; after each axis, if the AABB
   `m_position ± 0.3` overlaps (callback returns true) that axis move is undone. No gravity.
4. `view = lookAt(m_position, m_position + m_front, m_up)`.

Other API: `updateProjection(float w, float h)`, `getViewMatrix()`, `getProjectionMatrix()`,
`get/setPosition`, `setCollisionCallback(std::function<bool(const vec3& min, const vec3& max)>)`,
`isSphereVisible(center, radius)` / `isAabbVisible(box)` / `getFrustum()`: real frustum culling with planes extracted from projection × view (`mge::Frustum`, #24).

Members are `protected`: `m_position`, `m_front`, `m_up`, `m_right`, `m_worldUp`,
`m_yaw`, `m_pitch`, `m_movementSpeed`, `m_mouseSensitivity`, plus `m_width/m_height/m_fov/m_projectionMatrix/m_viewMatrix` and
`m_velocity/m_isGrounded/m_collisionCallback`.

Note: the view matrix is first computed in `update()`; before the first update it is
uninitialised.

## `FirstPersonCamera : Camera3D`

`update(dt)`:
- Mouse look always (ignores cursor lock state).
- `moveForward`/`moveRight` on the XZ plane (pitch removed), `sprint` ×1.5.
- `jump` while grounded → `velocity.y = 6.5`.
- With collision callback: player AABB is `m_position + [(-0.3,-1.5,-0.3) .. (0.3,0.3,0.3)]`
  (eye 1.5 above feet). X then Z resolved like Camera3D; gravity `-18 u/s²`; on downward hit
  snaps feet to the block top (`floor(min.y + 0.5) + 0.5`) and sets grounded; upward hit
  zeroes velocity.
- Without callback: spectator movement (gravity not applied, but jump velocity would persist).

⚠️ It **re‑declares** `m_velocity`, `m_isGrounded`, `m_collisionCallback` and
`setCollisionCallback`, shadowing Camera3D's. Always call `setCollisionCallback` on a
`FirstPersonCamera` typed reference, never through `Camera3D&`.

To switch SandboxApp to walking mode: change the member type `mge::Camera3D camera;` to
`mge::FirstPersonCamera camera;` and include its header.
