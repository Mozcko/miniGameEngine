# Python Bindings (`import mge`) Reference & Guide

The `mge` Python package provides idiomatic, high-performance Python 3.10+ bindings for `miniGameEngine`. Built with [nanobind](https://github.com/wjakob/nanobind) and packaged via [scikit-build-core](https://scikit-build-core.readthedocs.io/), it enables scripting games, rapid prototyping, headless simulations, and unit testing while preserving the raw performance of the underlying C++17 engine.

---

## 1. Installation & Build

### Requirements
- **Python**: 3.10, 3.11, 3.12, 3.13, or 3.14
- **C++ Compiler**: GCC 10+ or Clang 12+ supporting C++17
- **CMake**: 3.20+ and Ninja build tool
- **System Libraries**: Standard dependencies for Linux (see `scripts/ci-install-deps.sh`), including SDL3 development libraries (or built from source by CMake).

### Local Source Installation
Install directly in your active virtual environment or user site:
```bash
# Standard editable or in-place installation:
pip install -v -e .

# Or install release wheel:
pip install -v .

# With optional test tools (pytest, numpy):
pip install -v ".[test]"
```

### Git Tag Installation
Install directly from GitHub repository tags:
```bash
pip install "git+https://github.com/Mozcko/miniGameEngine@v1.0.0"
```

### Building via CMake Directly
When hacking on the C++ bindings directly:
```bash
cmake -B build -G Ninja -DMGE_BUILD_PYTHON=ON
cmake --build build --target _mge
export PYTHONPATH="$(pwd)/bindings/python:$(pwd)/build/bindings/python"
```

---

## 2. Architecture & Design Principles

### Direct C++ nanobind Layer
Following **ADR 0006**, the Python bindings wrap the engine's internal C++17 API directly using `nanobind` rather than calling through the C ABI (`mge_c`). This provides:
- Negligible call overhead and minimal binary size.
- Direct standard library and buffer protocol integration (`Py_buffer`).
- Idiomatic object-oriented classes with operator overloading.
- Full type stubs (`.pyi`) generated via `nanobind.stubgen`.

### GIL & Concurrency Rules
1. **GIL Release in `Application.run()`**: When `app.run()` starts the engine loop, it releases the Python Global Interpreter Lock (`nb::call_guard<nb::gil_scoped_release>()`).
2. **Synchronous Callbacks**: Every hook into Python (`on_init`, `on_update`, `on_fixed_update`, `on_render`, `on_shutdown`, and ECS systems) re-acquires the GIL (`nb::gil_scoped_acquire`).
3. **Main-Thread Exclusivity**: Like the C++ engine, all engine lifecycle and rendering operations execute strictly on the main thread.
4. **Worker Thread Concurrency**: Python background threads (`threading.Thread`) run concurrently during engine execution without deadlocking. Python callbacks are never invoked from internal engine `JobSystem` worker threads.

### GPU Handle Lifetime & Shutdown Safety
Every GPU mesh and texture created through Python is registered with the engine's `GpuResourceTracker`.
- On application shutdown or device destruction, all still-alive GPU handles are safely invalidated before destroying the device.
- In Debug builds, any unreleased handles generate a warning: `1 GpuMesh released at shutdown – release them in onShutdown`.
- Calling `renderer.draw_mesh()` or `renderer.draw_image()` on an invalidated handle raises `mge.EngineError`.
- `mesh.destroy()` and `texture.destroy()` are safe, idempotent no-ops post-shutdown.

---

## 3. Quickstart Example

Here is a simple interactive game with a 3D spinning cube, procedural audio, and 2D UI:

```python
import math
import mge

class SpinningCubeGame(mge.Application):
    def __init__(self):
        super().__init__(title="Spinning Cube", width=1280, height=720, vsync=True)
        self.cube = None
        self.coin_sfx = None
        self.rot_y = 0.0

    def on_init(self):
        self.input.bind_action("jump", "Space")
        self.input.bind_action("quit", "Escape")

        # Procedural cube mesh and audio sound effect
        self.cube = self.renderer.create_cube(size=1.5)
        self.coin_sfx = self.audio.create_sfx(mge.SfxPreset.COIN)

        # Camera setup
        view = mge.Mat4.look_at(mge.Vec3(0, 2, 4), mge.Vec3(0, 0, 0), mge.Vec3(0, 1, 0))
        proj = mge.Mat4.perspective(math.radians(60.0), 16.0 / 9.0, 0.1, 100.0)
        self.renderer.set_camera(view, proj)

        return True

    def on_update(self, dt):
        if self.input.is_action_pressed("quit"):
            self.request_stop()

        if self.input.is_action_pressed("jump"):
            self.audio.play(self.coin_sfx)

        self.rot_y += dt * 1.5

    def on_render(self):
        # 3D Render
        model = mge.Mat4.rotate(self.rot_y, mge.Vec3(0, 1, 0))
        self.renderer.draw_mesh(self.cube, model)

        # 2D Immediate UI
        self.renderer.draw_rect(20, 20, 320, 50, mge.Vec4(0, 0, 0, 0.7))
        self.renderer.draw_text(f"FPS: {self.time.fps:.0f} | Space: Sound", 35, 35, 1.0, mge.Vec4(1, 1, 1, 1))

    def on_shutdown(self):
        # Explicit cleanup removes GPU shutdown warnings
        if self.cube:
            self.cube.destroy()

if __name__ == "__main__":
    app = SpinningCubeGame()
    raise SystemExit(app.run())
```

---

## 4. API Reference

### Lifecycle & Application (`mge.Application`)
Subclass `mge.Application` to define your game:
- `__init__(title="miniGameEngine", width=1280, height=720, headless=False, modules=15, tick_rate=60, max_frames=0, vsync=True)`
- Lifecycle Hooks:
  - `on_init() -> bool`: Executed after subsystem initialization. Return `False` to abort startup.
  - `on_fixed_update(fixed_dt: float)`: Runs at fixed intervals (by default 60 Hz) for deterministic physics and logic.
  - `on_update(dt: float)`: Runs once per frame with variable scaled delta time.
  - `on_render()`: Runs once per frame to issue draw commands.
  - `on_shutdown()`: Runs before destroying engine modules and GPU devices.
- Control Methods:
  - `run() -> int`: Starts game loop; releases GIL.
  - `request_stop()`: Signals the loop to exit after the current frame.
  - `set_manual_clock(auto_step=1.0/60.0)`: Replaces steady clock with deterministic manual clock for tests and headless simulations.
- Properties:
  - `time`: `mge.Time` (`delta`, `fixed_delta`, `alpha`, `elapsed`, `frame_count`, `fps`, `time_scale`).
  - `input`: `mge.Input` (`bind_action`, `bind_axis`, `is_action_down`, `is_action_pressed`, `inject_key`, etc.).
  - `world`: `mge.World` (ECS coordinator).
  - `renderer`: `mge.Renderer` (Mesh, Texture, UI drawing).
  - `audio`: `mge.Audio` (Sound effect and music playback).
  - `viewport`: `tuple[int, int]` (Framebuffer dimensions).

### Linear Algebra & Math
Value types with operator overloading, arithmetic, and NumPy zero-copy buffer protocol:
- **`mge.Vec2(x, y)`**: `.x`, `.y`, `+`, `-`, `*`, `/`, `dot()`, `length()`, `norm()`, `normalized()`.
- **`mge.Vec3(x, y, z)`**: `.x`, `.y`, `.z`, `+`, `-`, `*`, `/`, `cross()`, `dot()`, `length()`, `norm()`, `normalized()`.
- **`mge.Vec4(x, y, z, w)`**: `.x`, `.y`, `.z`, `.w`, `+`, `-`, `*`, `/`, `dot()`, `length()`, `norm()`, `normalized()`.
- **`mge.Quat(x=0, y=0, z=0, w=1)`**: Quaternions representing 3D rotations.
  - `Quat.identity()`, `Quat.angle_axis(angle_rad, axis_vec3)`, `Quat.from_euler(pitch, yaw, roll)`.
  - `.x`, `.y`, `.z`, `.w`, `*` (composition or vec3 transformation), `inverse()`, `conjugate()`, `to_euler()`, `normalized()`.
- **`mge.Mat4`**: 4x4 Transformation matrices.
  - `Mat4.identity()`, `Mat4.translation(vec3)`, `Mat4.scaling(vec3)`, `Mat4.rotation(rad, axis)`.
  - `Mat4.perspective(fov_rad, aspect, near, far)`, `Mat4.look_at(eye, center, up)`.
  - `m1 * m2` or `m1 @ m2`, `m * v4`, indexing via `m[row, col]` or linear offset `m[0..15]`.
- **NumPy Zero-Copy Interoperability**:
  ```python
  import numpy as np
  v = mge.Vec3(1.0, 2.0, 3.0)
  arr = np.asarray(v)  # Shape (3,), dtype float32, shares memory
  arr[0] = 42.0
  assert v.x == 42.0
  ```

### ECS (Entity Component System)
Lightweight sparse-set ECS supporting built-in and pure Python components:
- **`mge.Entity`**: 64-bit handle. `world.create()` and `world.destroy(entity)`.
- **Built-in Components**:
  - `mge.Transform(position, rotation, scale)`
  - `mge.Camera(fov_y, near_plane, far_plane, is_orthographic, ortho_size)` (also `.fov` in degrees)
  - `mge.CharacterController(mode, speed, jump_speed, gravity, is_grounded)`
  - `mge.RigidBody(velocity, mass, restitution, gravity_scale)`
- **Python Custom Components**:
  Any Python class can be used as an ECS component:
  ```python
  class Velocity:
      def __init__(self, vx, vy, vz):
          self.v = mge.Vec3(vx, vy, vz)

  e = world.create()
  world.add(e, Velocity(1, 0, 0))
  assert world.has(e, Velocity)
  vel = world.get(e, Velocity)
  ```
- **Automatic Weakref Safety**: When an entity is destroyed or a component removed, all Python component instances are unreferenced immediately.
- **Multi-Component Queries (`world.each`)**:
  ```python
  # Iterator / Generator syntax:
  for ent, tr, vel in world.each(mge.Transform, Velocity):
      tr.position += vel.v * dt

  # Callback syntax:
  world.each(lambda ent, tr, vel: print(ent, tr.position), mge.Transform, Velocity)
  ```
- **Systems (`world.add_system`)**:
  ```python
  def move_system(dt):
      for ent, tr, vel in world.each(mge.Transform, Velocity):
          tr.position.x += vel.v.x * dt

  world.add_system(mge.SystemPhase.UPDATE, "MovementSystem", move_system)
  ```

### Graphics & Immediate 2D UI
- **`mge.Mesh`**:
  - `Mesh.create_cube(size=1.0)`
  - `Mesh.create_sphere(radius=0.5, rings=16, sectors=16)`
  - `Mesh.create_plane(width=1.0, depth=1.0)`
  - `Mesh.create_cylinder(radius=0.5, height=1.0, sectors=16)`
  - `Mesh.upload(vertices_list, indices_list)`
  - `.destroy()`, `.is_alive`
- **`mge.Texture2D`**:
  - `Texture2D.create_rgba(width, height, bytes)`
  - `Texture2D.create_solid(width, height, color_vec4)`
  - `.destroy()`, `.is_alive`, `.width`, `.height`
- **`mge.Renderer`**:
  - `renderer.set_camera(view_mat4, proj_mat4)`
  - `renderer.set_lighting(lighting)`
  - `renderer.set_time_of_day(hour)`
  - `renderer.draw_mesh(mesh, transform_mat4, tint=Vec4(1,1,1,1))`
  - `renderer.draw_rect(x, y, w, h, color)`
  - `renderer.draw_rect_outline(x, y, w, h, color, thickness=1.0)`
  - `renderer.drawLine(x0, y0, x1, y1, color, thickness=1.0)`
  - `renderer.draw_text(text, x, y, scale=1.0, color=Vec4(1,1,1,1))`
  - `renderer.draw_image(texture, x, y, w, h, color=Vec4(1,1,1,1))`

### Procedural Audio & SFX
- **`mge.Sample`**:
  - `Sample.create_sfx(mge.SfxPreset.COIN, seed=0)`
  - `Sample.create_sfx_custom(sfx_params)`
  - `Sample.load_wav("path/to/sound.wav")`
  - `.frame_count`, `.rms`, `.channels`, `.sample_rate`, `.is_alive`, `.destroy()`
- **Presets (`mge.SfxPreset`)**: `JUMP`, `COIN`, `HURT`, `EXPLOSION`, `POWER_UP`, `CLICK`.
- **Playback (`mge.Audio`)**:
  - `audio.play(sample, gain=1.0, pan=0.0)`
  - `audio.play_sfx(mge.SfxPreset.JUMP)`
  - `audio.play_song("track_name", fade_seconds=0.0)`
  - `audio.set_master_volume(vol)`, `audio.set_sfx_volume(vol)`, `audio.set_music_volume(vol)`, `audio.set_mute(bool)`

### Voxel Engine
- **`mge.BlockRegistry`**:
  - `id = reg.register_block("dirt", mge.BlockDesc(is_solid=True, is_opaque=True))`
  - `reg.name(id)`, `reg.is_solid(id)`, `reg.is_opaque(id)`, `reg.find("name")`, `BlockRegistry.AIR`.
- **`mge.VoxelWorld(registry, generator=None)`**:
  - `generator(x, y, z) -> block_id`: Pure Python terrain generation function.
  - `world.get_block(x, y, z) -> block_id`
  - `world.set_block(x, y, z, block_id)`
  - `world.load_chunk(cx, cy, cz)`
  - `hit = world.raycast(origin, direction, max_dist=50.0)` -> `RaycastResult` (`hit`, `point`, `block_pos`, `normal`, `distance`, `block_id`).

---

## 5. Development & Testing

### Running Tests
Execute the pytest suite:
```bash
PYTHONPATH=bindings/python pytest bindings/python/tests
```

### Type Stub Generation
Update the static type stubs when C++ bindings change:
```bash
PYTHONPATH=bindings/python python3 -m nanobind.stubgen -m mge._mge -O bindings/python/mge
```
Verify stubs match checked-in `.pyi` files.
