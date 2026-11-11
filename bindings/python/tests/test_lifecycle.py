import mge


class LifecycleApp(mge.Application):
    def __init__(self, max_frames=5):
        super().__init__(title="LifecycleTest", headless=True, max_frames=max_frames)
        self.set_manual_clock(1.0 / 60.0)
        self.init_called = False
        self.fixed_count = 0
        self.update_count = 0
        self.render_count = 0
        self.shutdown_called = False

    def on_init(self):
        self.init_called = True
        return True

    def on_fixed_update(self, dt):
        self.fixed_count += 1

    def on_update(self, dt):
        self.update_count += 1

    def on_render(self):
        self.render_count += 1

    def on_shutdown(self):
        self.shutdown_called = True


def test_application_lifecycle():
    app = LifecycleApp(max_frames=5)
    exit_code = app.run()
    assert exit_code == 0
    assert app.init_called
    assert app.fixed_count == 5
    assert app.update_count == 5
    assert app.render_count == 5
    assert app.shutdown_called
    assert app.time.frame_count == 5


def test_application_request_stop():
    class StopApp(mge.Application):
        def __init__(self):
            super().__init__(title="StopTest", headless=True, max_frames=100)
            self.set_manual_clock(1.0 / 60.0)
            self.frames = 0

        def on_update(self, dt):
            self.frames += 1
            if self.frames == 3:
                self.request_stop()

    app = StopApp()
    exit_code = app.run()
    assert exit_code == 0
    assert app.frames == 3
    assert not app.is_running


def test_application_abort_init():
    class AbortApp(mge.Application):
        def __init__(self):
            super().__init__(title="AbortTest", headless=True)

        def on_init(self):
            return False

    app = AbortApp()
    exit_code = app.run()
    assert exit_code != 0
