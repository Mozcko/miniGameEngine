import threading
import time
import mge
import pytest


def test_gil_concurrency_no_deadlock():
    worker_counter = 0
    stop_worker = False

    def background_thread():
        nonlocal worker_counter
        while not stop_worker:
            worker_counter += 1
            time.sleep(0.001)

    t = threading.Thread(target=background_thread)
    t.start()

    class ConcurrencyApp(mge.Application):
        def __init__(self):
            super().__init__(title="GilTest", headless=True, max_frames=10)
            self.set_manual_clock(1.0 / 60.0)

        def on_update(self, dt):
            # Sleep slightly to let the background thread run
            time.sleep(0.002)

    app = ConcurrencyApp()
    exit_code = app.run()

    stop_worker = True
    t.join(timeout=2.0)
    assert not t.is_alive()
    assert exit_code == 0
    assert worker_counter > 0, "Background Python thread ran during app.run()"
