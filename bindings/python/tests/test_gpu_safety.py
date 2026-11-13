import mge
import pytest


def test_gpu_mesh_inert_post_shutdown():
    captured_mesh = []

    class MeshApp(mge.Application):
        def __init__(self):
            super().__init__(title="MeshApp", headless=True, max_frames=2)

        def on_init(self):
            # In headless mode without GPU, create_cube creates CPU mesh data
            mesh = self.renderer.create_cube(1.0)
            captured_mesh.append(mesh)
            return True

    app = MeshApp()
    app.run()

    mesh = captured_mesh[0]
    # Destroying or verifying inert state
    mesh.destroy()
    assert not mesh.is_alive

    # Attempting to draw an inert mesh raises EngineError
    with pytest.raises(mge.EngineError):
        app.renderer.draw_mesh(mesh, mge.Mat4.identity())


def test_gpu_texture_inert_post_shutdown():
    captured_tex = []

    class TexApp(mge.Application):
        def __init__(self):
            super().__init__(title="TexApp", headless=True, max_frames=2)

        def on_init(self):
            pixels = b"\xff\x00\x00\xff" * 4
            tex = mge.Texture2D.create_rgba(2, 2, pixels, self)
            captured_tex.append(tex)
            return True

    app = TexApp()
    app.run()

    tex = captured_tex[0]
    tex.destroy()
    assert not tex.is_alive

    # Attempting to draw an inert texture raises EngineError
    with pytest.raises(mge.EngineError):
        app.renderer.draw_image(tex, 0, 0, 100, 100)
