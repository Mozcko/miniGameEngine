"""miniGameEngine Python hello example.

Displays a spinning cube with sky, directional lighting, and 2D text.
Press SPACE to play a coin sound, ESC to quit.
"""

import math
import mge


class HelloGame(mge.Application):
    def __init__(self, headless=False, max_frames=0):
        super().__init__(
            title="miniGameEngine - Python Hello",
            width=1280,
            height=720,
            headless=headless,
            max_frames=max_frames,
            vsync=True,
        )
        self.cube = None
        self.coin_sfx = None
        self.rotation_y = 0.0

    def on_init(self):
        # Bind controls
        self.input.bind_action("coin", "Space")
        self.input.bind_action("quit", "Escape")

        # Procedural mesh and audio
        self.cube = self.renderer.create_cube(1.5)
        self.coin_sfx = self.audio.create_sfx(mge.SfxPreset.COIN)

        # Set up camera looking at center
        eye = mge.Vec3(0.0, 2.0, 4.0)
        center = mge.Vec3(0.0, 0.0, 0.0)
        up = mge.Vec3(0.0, 1.0, 0.0)
        view = mge.Mat4.look_at(eye, center, up)
        proj = mge.Mat4.perspective(math.radians(60.0), 1280.0 / 720.0, 0.1, 100.0)
        self.renderer.set_camera(view, proj)

        # Lighting and sky
        lighting = mge.Lighting(
            sun_dir=mge.Vec3(0.5, 1.0, 0.3),
            sun_color=mge.Vec3(1.0, 0.95, 0.8),
            sun_intensity=1.2,
            sky_ambient=mge.Vec3(0.15, 0.2, 0.3),
            ground_ambient=mge.Vec3(0.1, 0.08, 0.05),
        )
        self.renderer.set_lighting(lighting)
        self.renderer.set_time_of_day(14.0)

        return True

    def on_update(self, dt):
        if self.input.is_action_pressed("quit"):
            self.request_stop()

        if self.input.is_action_pressed("coin"):
            self.audio.play(self.coin_sfx)

        # Spin the cube
        self.rotation_y += dt * 1.5

    def on_render(self):
        if not self.cube or not self.cube.is_alive:
            return

        # 3D Spinning cube
        model = (
            mge.Mat4.translation(mge.Vec3(0.0, 0.0, 0.0))
            * mge.Mat4.rotate(self.rotation_y, mge.Vec3(0.0, 1.0, 0.0))
            * mge.Mat4.rotate(math.radians(25.0), mge.Vec3(1.0, 0.0, 0.0))
        )
        self.renderer.draw_mesh(self.cube, model)

        # 2D UI Overlay
        self.renderer.draw_rect(20.0, 20.0, 360.0, 60.0, mge.Vec4(0.0, 0.0, 0.0, 0.6))
        self.renderer.draw_rect_outline(20.0, 20.0, 360.0, 60.0, mge.Vec4(1.0, 1.0, 1.0, 0.8), 2.0)
        self.renderer.draw_text("miniGameEngine Python", 35.0, 32.0, 1.0, mge.Vec4(1.0, 1.0, 1.0, 1.0))
        self.renderer.draw_text(
            f"FPS: {self.time.fps:.0f} | Space: Coin | Esc: Quit",
            35.0,
            55.0,
            0.8,
            mge.Vec4(0.8, 0.8, 0.8, 1.0),
        )

    def on_shutdown(self):
        if self.cube:
            self.cube.destroy()


if __name__ == "__main__":
    import os
    import sys

    headless = "--headless" in sys.argv or os.environ.get("SDL_VIDEODRIVER") == "offscreen"
    max_frames = int(os.environ.get("MGE_MAX_FRAMES", "0"))
    game = HelloGame(headless=headless, max_frames=max_frames)
    exit_code = game.run()
    raise SystemExit(exit_code)
