import weakref
import mge
import pytest


class Velocity:
    def __init__(self, vx, vy, vz):
        self.vx = vx
        self.vy = vy
        self.vz = vz


class Health:
    def __init__(self, hp):
        self.hp = hp


def test_entity_creation_destruction():
    world = mge.World()
    e1 = world.create()
    e2 = world.create()
    assert world.is_alive(e1)
    assert world.is_alive(e2)
    assert e1 != e2

    world.destroy(e1)
    assert not world.is_alive(e1)
    assert world.is_alive(e2)


def test_builtin_components():
    world = mge.World()
    e = world.create()

    # Transform
    t = mge.Transform(position=mge.Vec3(1.0, 2.0, 3.0))
    world.add(e, t)
    assert world.has(e, mge.Transform)

    got_t = world.get(e, mge.Transform)
    assert got_t.position.x == pytest.approx(1.0)
    got_t.position.x = 99.0

    got_t2 = world.get(e, mge.Transform)
    assert got_t2.position.x == pytest.approx(99.0)

    # Camera
    cam = mge.Camera()
    cam.fov = 75.0
    cam.near_plane = 0.5
    world.add(e, cam)
    assert world.has(e, mge.Camera)
    assert world.get(e, mge.Camera).fov == pytest.approx(75.0)

    # RigidBody
    rb = mge.RigidBody()
    rb.mass = 5.0
    world.add(e, rb)
    assert world.has(e, mge.RigidBody)
    assert world.get(e, mge.RigidBody).mass == pytest.approx(5.0)

    # Remove
    world.remove(e, mge.Transform)
    assert not world.has(e, mge.Transform)
    with pytest.raises(KeyError):
        world.get(e, mge.Transform)


def test_python_custom_components_and_weakref():
    world = mge.World()
    e = world.create()

    v = Velocity(10.0, 20.0, 30.0)
    wr_v = weakref.ref(v)
    world.add(e, v)
    del v

    assert wr_v() is not None
    assert world.has(e, Velocity)
    retrieved = world.get(e, Velocity)
    assert retrieved.vx == pytest.approx(10.0)

    # Destroy entity should release Python component
    world.destroy(e)
    del retrieved
    assert wr_v() is None


def test_component_remove_weakref():
    world = mge.World()
    e = world.create()

    h = Health(100)
    wr_h = weakref.ref(h)
    world.add(e, h)
    del h

    assert wr_h() is not None
    world.remove(e, Health)
    assert not world.has(e, Health)
    assert wr_h() is None


def test_world_each_query():
    world = mge.World()
    e1 = world.create()
    e2 = world.create()
    e3 = world.create()

    world.add(e1, mge.Transform(position=mge.Vec3(1.0, 0.0, 0.0)))
    world.add(e1, Velocity(1.0, 0.0, 0.0))

    world.add(e2, mge.Transform(position=mge.Vec3(2.0, 0.0, 0.0)))
    world.add(e2, Velocity(2.0, 0.0, 0.0))

    world.add(e3, mge.Transform(position=mge.Vec3(3.0, 0.0, 0.0)))
    # e3 has no Velocity

    # Generator/list form
    results = list(world.each(mge.Transform, Velocity))
    assert len(results) == 2
    entities = [ent for ent, _, _ in results]
    assert e1 in entities
    assert e2 in entities
    assert e3 not in entities

    # Callback form
    visited = []
    world.each(lambda ent, tr, vel: visited.append((ent, tr.position.x, vel.vx)),
               mge.Transform, Velocity)
    assert len(visited) == 2
    xs = [x for _, x, _ in visited]
    assert 1.0 in xs
    assert 2.0 in xs


def test_systems():
    class SimApp(mge.Application):
        def __init__(self):
            super().__init__(title="SimTest", headless=True, max_frames=3)
            self.set_manual_clock(1.0 / 60.0)
            self.entity = None

        def on_init(self):
            self.entity = self.world.create()
            self.world.add(self.entity, mge.Transform(position=mge.Vec3(0.0, 0.0, 0.0)))
            self.world.add(self.entity, Velocity(60.0, 0.0, 0.0))

            def move_system(dt):
                for _, tr, vel in self.world.each(mge.Transform, Velocity):
                    tr.position.x += vel.vx * dt

            self.world.add_system(mge.SystemPhase.UPDATE, "MovementSystem", move_system)
            return True

    app = SimApp()
    app.run()
    tr = app.world.get(app.entity, mge.Transform)
    # 3 frames of 1/60s with vx=60 -> 3.0
    assert tr.position.x == pytest.approx(3.0)
