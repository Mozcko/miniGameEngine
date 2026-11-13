"""miniGameEngine Python headless deterministic simulation example.

Demonstrates ECS with Python components, manual clock stepping,
and deterministic fixed-update movement for 100 entities.
"""

import mge


class Velocity:
    def __init__(self, vx: float, vy: float, vz: float):
        self.vx = vx
        self.vy = vy
        self.vz = vz


class HeadlessSim(mge.Application):
    def __init__(self, steps: int = 120):
        super().__init__(
            title="HeadlessSim",
            headless=True,
            max_frames=steps,
        )
        self.set_manual_clock(1.0 / 60.0)
        self.entities = []

    def on_init(self):
        # Spawn 100 entities along a grid
        for i in range(100):
            e = self.world.create()
            pos = mge.Vec3(float(i % 10), 0.0, float(i // 10))
            self.world.add(e, mge.Transform(position=pos))
            self.world.add(e, Velocity(0.5, 0.0, 1.0))
            self.entities.append(e)

        # Movement system in fixed update phase
        def movement_system(dt):
            for _, tr, vel in self.world.each(mge.Transform, Velocity):
                tr.position.x += vel.vx * dt
                tr.position.z += vel.vz * dt

        self.world.add_system(mge.SystemPhase.FIXED, "MovementSystem", movement_system)
        return True

    def on_shutdown(self):
        first_tr = self.world.get(self.entities[0], mge.Transform)
        last_tr = self.world.get(self.entities[-1], mge.Transform)
        print(f"Simulation completed ({self.time.frame_count} frames).")
        print(f"  Entity 0:  pos=({first_tr.position.x:.2f}, {first_tr.position.z:.2f})")
        print(f"  Entity 99: pos=({last_tr.position.x:.2f}, {last_tr.position.z:.2f})")


if __name__ == "__main__":
    sim = HeadlessSim(steps=60)
    exit_code = sim.run()
    raise SystemExit(exit_code)
