import mge
import pytest


def test_input_actions_and_injection():
    class InputApp(mge.Application):
        def __init__(self):
            super().__init__(title="InputTest", headless=True, max_frames=3)
            self.set_manual_clock(1.0 / 60.0)

        def on_init(self):
            self.input.bind_action("jump", "Space")
            self.input.bind_axis("move_x", "A", "D")
            return True

        def on_update(self, dt):
            if self.time.frame_count == 1:
                assert not self.input.is_action_down("jump")
                self.input.inject_key("Space", True)
                assert self.input.is_action_down("jump")
                assert self.input.is_action_pressed("jump")

                self.input.inject_key("D", True)
                assert self.input.get_axis("move_x") == pytest.approx(1.0)
            elif self.time.frame_count == 2:
                self.input.inject_key("D", False)
                self.input.inject_key("A", True)
                assert self.input.get_axis("move_x") == pytest.approx(-1.0)

                self.input.inject_key("Space", False)
                assert not self.input.is_action_down("jump")
                assert self.input.is_action_released("jump")

    app = InputApp()
    exit_code = app.run()
    assert exit_code == 0
