// NOLINTBEGIN(readability-identifier-naming)
#include <mge/c/mge_c_internal.h>

#include <mge/core/Assets.h>
#include <mge/synth/Offline.h>

#include <SDL3/SDL.h>

#include <cmath>
#include <cstring>
#include <filesystem>
#include <string>

namespace {

    mge::Sample loadWavFile(const std::filesystem::path& path, int sampleRate) {
        const std::filesystem::path full = path.is_absolute() ? path : mge::assetPath(path.generic_string());
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
        const bool ok = SDL_ConvertAudioSamples(&source, data, static_cast<int>(length), &target, &converted,
                                                &convertedLength);
        SDL_free(data);
        if (!ok) {
            return mge::Sample{};
        }
        mge::Sample sample;
        sample.channels = channels;
        sample.data.resize(static_cast<std::size_t>(convertedLength) / sizeof(float));
        std::memcpy(sample.data.data(), converted, sample.data.size() * sizeof(float));
        SDL_free(converted);
        return sample;
    }

} // namespace

extern "C" {

mge_audio_sample* mge_audio_sfx_create_preset(mge_app* app, mge_sfx_preset preset, uint32_t seed) {
    mge::SfxParams params;
    switch (preset) {
    case MGE_SFX_JUMP: params = mge::sfx::jump(seed); break;
    case MGE_SFX_COIN: params = mge::sfx::coin(seed); break;
    case MGE_SFX_HURT: params = mge::sfx::hurt(seed); break;
    case MGE_SFX_EXPLOSION: params = mge::sfx::explosion(seed); break;
    case MGE_SFX_POWER_UP: params = mge::sfx::powerUp(seed); break;
    case MGE_SFX_CLICK: params = mge::sfx::click(seed); break;
    default: params = mge::sfx::coin(seed); break;
    }

    auto* sample = new mge_audio_sample();
    sample->app = app;
    int rate = (app && app->audio) ? app->audio->mixer().sampleRate() : mge::kDefaultSampleRate;
    sample->sample = mge::renderSfx(params, rate, seed);
    return sample;
}

mge_audio_sample* mge_audio_sfx_create_custom(mge_app* app, const mge_sfx_params* p) {
    if (!p) return nullptr;
    mge::SfxParams params;
    params.wave = static_cast<mge::Waveform>(p->wave_type);
    params.attack = p->attack_time;
    params.sustain = p->sustain_time;
    params.punch = p->sustain_punch;
    params.decay = p->decay_time;
    params.baseFrequency = p->start_frequency;
    params.minFrequency = p->min_frequency;
    params.slide = p->slide;
    params.deltaSlide = p->delta_slide;
    params.duty = p->square_duty;
    params.dutySweep = p->duty_sweep;
    params.vibratoDepth = p->vibrato_depth;
    params.vibratoSpeed = p->vibrato_speed;
    params.arpeggioMultiplier = p->change_amount;
    params.arpeggioTime = p->change_speed;
    params.lowPassCutoff = p->lp_filter_cutoff;
    params.lowPassSweep = p->lp_filter_sweep;
    params.lowPassResonance = p->lp_filter_resonance;
    params.highPassCutoff = p->hp_filter_cutoff;
    params.highPassSweep = p->hp_filter_sweep;
    params.phaserOffset = p->phaser_offset;
    params.phaserSweep = p->phaser_sweep;
    params.repeatTime = p->repeat_speed;

    auto* sample = new mge_audio_sample();
    sample->app = app;
    int rate = (app && app->audio) ? app->audio->mixer().sampleRate() : mge::kDefaultSampleRate;
    sample->sample = mge::renderSfx(params, rate);
    return sample;
}

mge_audio_sample* mge_audio_sample_load_wav(mge_app* app, const char* file_path) {
    if (!file_path) return nullptr;
    int rate = (app && app->audio) ? app->audio->mixer().sampleRate() : mge::kDefaultSampleRate;
    mge::Sample s = loadWavFile(file_path, rate);
    if (s.empty()) return nullptr;

    auto* sample = new mge_audio_sample();
    sample->app = app;
    sample->sample = std::move(s);
    return sample;
}

void mge_audio_sample_destroy(mge_audio_sample* sample) {
    delete sample;
}

size_t mge_audio_sample_frame_count(const mge_audio_sample* sample) {
    if (!sample) return 0;
    return sample->sample.frameCount();
}

float mge_audio_sample_rms(const mge_audio_sample* sample) {
    if (!sample || sample->sample.empty()) return 0.0f;
    return mge::analysis::rms(sample->sample.data);
}

mge_sound_instance mge_audio_play(mge_app* app, mge_audio_sample* sample, float gain, float pan) {
    if (!app || !app->audio || !sample || sample->sample.empty()) return 0;
    if (sample->sampleId == mge::kInvalidSample) {
        sample->sampleId = app->audio->mixer().addSample(sample->sample);
    }
    if (sample->sampleId != mge::kInvalidSample) {
        if (app->audio->play(sample->sampleId, gain, pan)) {
            return static_cast<mge_sound_instance>(sample->sampleId);
        }
    }
    return 0;
}

void mge_audio_song_play(mge_app* app, const char* song_name, float fade_seconds) {
    if (!app || !app->audio || !song_name) return;
    std::string s(song_name);
    mge::Song song;
    if (s == "platformer" || s == "platformerTheme") {
        song = mge::songs::platformerTheme();
    } else if (s == "night" || s == "sandboxNight") {
        song = mge::songs::sandboxNight();
    } else {
        song = mge::songs::sandboxAmbient();
    }

    mge::SongId id = app->audio->addSong(song);
    app->audio->playSong(id, fade_seconds);
}

void mge_audio_song_stop(mge_app* app, float fade_seconds) {
    if (!app || !app->audio) return;
    app->audio->stopMusic(fade_seconds);
}

void mge_audio_set_master_volume(mge_app* app, float volume) {
    if (!app || !app->audio) return;
    app->audio->setMasterVolume(volume);
}

void mge_audio_set_sfx_volume(mge_app* app, float volume) {
    if (!app || !app->audio) return;
    app->audio->setBusVolume(mge::AudioBus::Effects, volume);
}

void mge_audio_set_music_volume(mge_app* app, float volume) {
    if (!app || !app->audio) return;
    app->audio->setBusVolume(mge::AudioBus::Music, volume);
}

void mge_audio_set_mute(mge_app* app, bool mute) {
    if (!app || !app->audio) return;
    app->audio->setMuted(mute);
}

} // extern "C"
// NOLINTEND(readability-identifier-naming)
