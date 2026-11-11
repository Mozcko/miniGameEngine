import mge
import pytest


def test_audio_sfx_presets():
    presets = [
        mge.SfxPreset.JUMP,
        mge.SfxPreset.COIN,
        mge.SfxPreset.HURT,
        mge.SfxPreset.EXPLOSION,
        mge.SfxPreset.POWER_UP,
        mge.SfxPreset.CLICK,
    ]

    for p in presets:
        s = mge.Sample.create_sfx(p, seed=42)
        assert s.is_alive
        assert s.frame_count > 0
        assert s.channels == 1
        assert s.sample_rate == 48000
        assert s.rms > 0.0


def test_audio_sfx_custom():
    params = mge.SfxParams()
    params.start_frequency = 500.0
    params.sustain_time = 0.2
    params.decay_time = 0.3
    s = mge.Sample.create_sfx_custom(params)
    assert s.is_alive
    assert s.frame_count > 0
    assert s.rms > 0.0


def test_audio_controls():
    class AudioApp(mge.Application):
        def __init__(self):
            super().__init__(title="AudioTest", headless=True, max_frames=2)

        def on_init(self):
            s = self.audio.create_sfx(mge.SfxPreset.COIN)
            self.audio.set_master_volume(0.8)
            self.audio.set_sfx_volume(0.7)
            self.audio.set_music_volume(0.5)
            self.audio.set_mute(False)

            # Playing in headless should safely no-op without error
            self.audio.play(s, gain=0.9, pan=0.0)
            self.audio.play_sfx(mge.SfxPreset.JUMP)
            return True

    app = AudioApp()
    exit_code = app.run()
    assert exit_code == 0
