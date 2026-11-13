#include "Audio.h"
#include "App.h"

#include <mge/core/Assets.h>

#include <SDL3/SDL.h>

#include <cmath>
#include <cstring>
#include <filesystem>

namespace mge::python {

    namespace {

        mge::Sample loadWavFile(const std::filesystem::path& path, int sampleRate) {
            const std::filesystem::path full =
                path.is_absolute() ? path : mge::assetPath(path.generic_string());
            SDL_AudioSpec source{};
            Uint8* data = nullptr;
            Uint32 length = 0;
            if (!SDL_LoadWAV(full.string().c_str(), &source, &data, &length)) {
                return mge::Sample{};
            }
            const int channels = std::min(static_cast<int>(source.channels), mge::kAudioChannels);
            const SDL_AudioSpec target{SDL_AUDIO_F32, channels, sampleRate};
            Uint8* converted = nullptr;
            int convertedLength = 0;
            const bool ok = SDL_ConvertAudioSamples(&source, data, static_cast<int>(length), &target,
                                                    &converted, &convertedLength);
            SDL_free(data);
            if (!ok) {
                return mge::Sample{};
            }
            mge::Sample sample;
            sample.channels = channels;
            sample.data.resize(static_cast<size_t>(convertedLength) / sizeof(float));
            std::memcpy(sample.data.data(), converted, sample.data.size() * sizeof(float));
            SDL_free(converted);
            return sample;
        }

    } // namespace

    std::shared_ptr<PySample> PySample::createSfx(PySfxPreset preset, uint32_t seed, mge::Application* app) {
        auto* pyApp = app ? static_cast<PyApplication*>(app) : PyApplication::activeApp();
        mge::SfxParams params;
        switch (preset) {
        case PySfxPreset::Jump: params = mge::sfx::jump(seed); break;
        case PySfxPreset::Coin: params = mge::sfx::coin(seed); break;
        case PySfxPreset::Hurt: params = mge::sfx::hurt(seed); break;
        case PySfxPreset::Explosion: params = mge::sfx::explosion(seed); break;
        case PySfxPreset::PowerUp: params = mge::sfx::powerUp(seed); break;
        case PySfxPreset::Click: params = mge::sfx::click(seed); break;
        default: params = mge::sfx::coin(seed); break;
        }

        const int rate = (pyApp && pyApp->audioModule()) ? pyApp->audioModule()->mixer().sampleRate()
                                                         : mge::kDefaultSampleRate;
        return std::make_shared<PySample>(mge::renderSfx(params, rate, seed));
    }

    std::shared_ptr<PySample> PySample::createSfxCustom(const PySfxParams& p, mge::Application* app) {
        auto* pyApp = app ? static_cast<PyApplication*>(app) : PyApplication::activeApp();
        mge::SfxParams params;
        params.wave = static_cast<mge::Waveform>(p.waveType);
        params.attack = p.attackTime;
        params.sustain = p.sustainTime;
        params.punch = p.sustainPunch;
        params.decay = p.decayTime;
        params.baseFrequency = p.startFrequency;
        params.minFrequency = p.minFrequency;
        params.slide = p.slide;
        params.deltaSlide = p.deltaSlide;
        params.duty = p.squareDuty;
        params.dutySweep = p.dutySweep;
        params.vibratoDepth = p.vibratoDepth;
        params.vibratoSpeed = p.vibratoSpeed;
        params.arpeggioMultiplier = p.changeAmount;
        params.arpeggioTime = p.changeSpeed;
        params.lowPassCutoff = p.lpFilterCutoff;
        params.lowPassSweep = p.lpFilterSweep;
        params.lowPassResonance = p.lpFilterResonance;
        params.highPassCutoff = p.hpFilterCutoff;
        params.highPassSweep = p.hpFilterSweep;
        params.phaserOffset = p.phaserOffset;
        params.phaserSweep = p.phaserSweep;
        params.repeatTime = p.repeatSpeed;

        const int rate = (pyApp && pyApp->audioModule()) ? pyApp->audioModule()->mixer().sampleRate()
                                                         : mge::kDefaultSampleRate;
        return std::make_shared<PySample>(mge::renderSfx(params, rate, 0));
    }

    std::shared_ptr<PySample> PySample::loadWav(const std::string& path, mge::Application* app) {
        auto* pyApp = app ? static_cast<PyApplication*>(app) : PyApplication::activeApp();
        const int rate = (pyApp && pyApp->audioModule()) ? pyApp->audioModule()->mixer().sampleRate()
                                                         : mge::kDefaultSampleRate;
        return std::make_shared<PySample>(loadWavFile(path, rate));
    }

    // --- PyAudio ---
    uint64_t PyAudio::play(const std::shared_ptr<PySample>& sample, float gain, float pan) {
        if (!m_app || !m_app->audioModule() || !sample || sample->sample().empty()) return 0;
        if (sample->m_sampleId == mge::kInvalidSample) {
            sample->m_sampleId = m_app->audioModule()->mixer().addSample(sample->sample());
        }
        if (sample->m_sampleId != mge::kInvalidSample) {
            if (m_app->audioModule()->play(sample->m_sampleId, gain, pan)) {
                return static_cast<uint64_t>(sample->m_sampleId);
            }
        }
        return 0;
    }

    uint64_t PyAudio::playSfx(PySfxPreset preset, uint32_t seed, float gain) {
        auto sample = createSfx(preset, seed);
        return play(sample, gain);
    }

    void PyAudio::playSong(const std::string& songName, float fadeSeconds) {
        if (!m_app || !m_app->audioModule()) return;
        mge::Song song;
        if (songName == "platformer" || songName == "platformerTheme") {
            song = mge::songs::platformerTheme();
        } else if (songName == "night" || songName == "sandboxNight") {
            song = mge::songs::sandboxNight();
        } else {
            song = mge::songs::sandboxAmbient();
        }
        const mge::SongId id = m_app->audioModule()->addSong(song);
        m_app->audioModule()->playSong(id, fadeSeconds);
    }

    void PyAudio::stopSong(float fadeSeconds) {
        if (!m_app || !m_app->audioModule()) return;
        m_app->audioModule()->stopMusic(fadeSeconds);
    }

    void PyAudio::setMasterVolume(float volume) {
        if (!m_app || !m_app->audioModule()) return;
        m_app->audioModule()->setMasterVolume(volume);
    }

    void PyAudio::setSfxVolume(float volume) {
        if (!m_app || !m_app->audioModule()) return;
        m_app->audioModule()->setBusVolume(mge::AudioBus::Effects, volume);
    }

    void PyAudio::setMusicVolume(float volume) {
        if (!m_app || !m_app->audioModule()) return;
        m_app->audioModule()->setBusVolume(mge::AudioBus::Music, volume);
    }

    void PyAudio::setMute(bool mute) {
        if (!m_app || !m_app->audioModule()) return;
        m_app->audioModule()->setMuted(mute);
    }

    std::shared_ptr<PySample> PyAudio::createSfx(PySfxPreset preset, uint32_t seed) {
        return PySample::createSfx(preset, seed, m_app);
    }

    std::shared_ptr<PySample> PyAudio::createSfxCustom(const PySfxParams& params) {
        return PySample::createSfxCustom(params, m_app);
    }

    std::shared_ptr<PySample> PyAudio::loadWav(const std::string& path) {
        return PySample::loadWav(path, m_app);
    }

    void initAudio(nanobind::module_& m) {
        namespace nb = nanobind;

        // --- SfxPreset ---
        nb::enum_<PySfxPreset>(m, "SfxPreset")
            .value("JUMP", PySfxPreset::Jump)
            .value("COIN", PySfxPreset::Coin)
            .value("HURT", PySfxPreset::Hurt)
            .value("EXPLOSION", PySfxPreset::Explosion)
            .value("POWER_UP", PySfxPreset::PowerUp)
            .value("CLICK", PySfxPreset::Click)
            .export_values();

        // --- SfxParams ---
        nb::class_<PySfxParams>(m, "SfxParams")
            .def(nb::init<>())
            .def_rw("wave_type", &PySfxParams::waveType)
            .def_rw("attack_time", &PySfxParams::attackTime)
            .def_rw("sustain_time", &PySfxParams::sustainTime)
            .def_rw("sustain_punch", &PySfxParams::sustainPunch)
            .def_rw("decay_time", &PySfxParams::decayTime)
            .def_rw("start_frequency", &PySfxParams::startFrequency)
            .def_rw("min_frequency", &PySfxParams::minFrequency)
            .def_rw("slide", &PySfxParams::slide)
            .def_rw("delta_slide", &PySfxParams::deltaSlide)
            .def_rw("square_duty", &PySfxParams::squareDuty)
            .def_rw("duty_sweep", &PySfxParams::dutySweep)
            .def_rw("vibrato_depth", &PySfxParams::vibratoDepth)
            .def_rw("vibrato_speed", &PySfxParams::vibratoSpeed)
            .def_rw("change_amount", &PySfxParams::changeAmount)
            .def_rw("change_speed", &PySfxParams::changeSpeed)
            .def_rw("lp_filter_cutoff", &PySfxParams::lpFilterCutoff)
            .def_rw("lp_filter_sweep", &PySfxParams::lpFilterSweep)
            .def_rw("lp_filter_resonance", &PySfxParams::lpFilterResonance)
            .def_rw("hp_filter_cutoff", &PySfxParams::hpFilterCutoff)
            .def_rw("hp_filter_sweep", &PySfxParams::hpFilterSweep)
            .def_rw("phaser_offset", &PySfxParams::phaserOffset)
            .def_rw("phaser_sweep", &PySfxParams::phaserSweep)
            .def_rw("repeat_speed", &PySfxParams::repeatSpeed);

        // --- Sample ---
        nb::class_<PySample>(m, "Sample")
            .def_static("create_sfx", &PySample::createSfx, nb::arg("preset"), nb::arg("seed") = 0,
                        nb::arg("app") = nullptr)
            .def_static("create_sfx_custom", &PySample::createSfxCustom, nb::arg("params"),
                        nb::arg("app") = nullptr)
            .def_static("load_wav", &PySample::loadWav, nb::arg("path"), nb::arg("app") = nullptr)
            .def_prop_ro("frame_count", &PySample::frameCount)
            .def_prop_ro("rms", &PySample::rms)
            .def_prop_ro("channels", &PySample::channels)
            .def_prop_ro("sample_rate", &PySample::sampleRate)
            .def_prop_ro("is_alive", &PySample::isAlive)
            .def("destroy", &PySample::destroy);

        // --- Audio ---
        nb::class_<PyAudio>(m, "Audio")
            .def("play", &PyAudio::play, nb::arg("sample"), nb::arg("gain") = 1.0f, nb::arg("pan") = 0.0f)
            .def("play_sfx", &PyAudio::playSfx, nb::arg("preset"), nb::arg("seed") = 0,
                 nb::arg("gain") = 1.0f)
            .def("play_song", &PyAudio::playSong, nb::arg("song_name"), nb::arg("fade_seconds") = 0.0f)
            .def("stop_song", &PyAudio::stopSong, nb::arg("fade_seconds") = 0.0f)
            .def("set_master_volume", &PyAudio::setMasterVolume, nb::arg("volume"))
            .def("set_sfx_volume", &PyAudio::setSfxVolume, nb::arg("volume"))
            .def("set_music_volume", &PyAudio::setMusicVolume, nb::arg("volume"))
            .def("set_mute", &PyAudio::setMute, nb::arg("mute"))
            .def("create_sfx", &PyAudio::createSfx, nb::arg("preset"), nb::arg("seed") = 0)
            .def("create_sfx_custom", &PyAudio::createSfxCustom, nb::arg("params"))
            .def("load_wav", &PyAudio::loadWav, nb::arg("path"));
    }

} // namespace mge::python
