#pragma once

#include <mge/core/Module.h>
#include <mge/synth/Mixer.h>
#include <mge/synth/Sfx.h>

#include <filesystem>
#include <vector>

struct SDL_AudioStream;

namespace mge {

    // Audio module (#46): connects a Mixer (mge_synth) to the default audio device using a
    // float32 stereo SDL_AudioStream. SDL invokes the mixer from its audio thread; the game talks to the
    // mixer through a lock-free queue. Without a device (or with SDL_AUDIODRIVER=dummy) the game continues
    // silently: this is not an error. MGE_MUTE=1 starts muted.
    //   auto* audio = getModule<mge::AudioModule>();
    //   audio->playTone({440.0f, 0.3f});                   // no files
    //   const SampleId jump = audio->loadWav("sounds/jump.wav");
    //   audio->play(jump);
    // Game thread: a single thread calls play/load/volume.
    class AudioModule : public Module {
    public:
        explicit AudioModule(int sampleRate = kDefaultSampleRate);
        ~AudioModule() override;

        const char* name() const override { return "Audio"; }
        bool onInit(Application& app) override;
        void onShutdown(Application& app) override;

        // Is a playback device active? (false: everything still works, just silently)
        bool hasDevice() const { return m_stream != nullptr; }
        Mixer& mixer() { return m_mixer; }

        // WAV → Sample at the mixer's sample rate (relative → mge::assetPath). kInvalidSample on failure
        SampleId loadWav(const std::filesystem::path& path);
        SampleId addSample(Sample sample) { return m_mixer.addSample(std::move(sample)); }
        // Renders an sfxr effect (#48) at the mixer's sample rate and registers it: mge::sfx::jump(), coin()…
        SampleId addSfx(const SfxParams& params, std::uint64_t seed = 0) {
            return m_mixer.addSample(renderSfx(params, m_mixer.sampleRate(), seed));
        }

        bool play(SampleId sample, float gain = 1.0f, float pan = 0.0f) {
            return m_mixer.playSample(sample, gain, pan);
        }
        bool playTone(const ToneDesc& tone) { return m_mixer.playTone(tone); }
        // A note from a synthesized instrument (#47); see Mixer::playNote
        VoiceHandle playNote(const Instrument& instrument, float frequency, float velocity = 1.0f,
                             float duration = 0.25f, AudioBus bus = AudioBus::Effects) {
            return m_mixer.playNote(instrument, frequency, velocity, duration, bus);
        }
        bool releaseNote(VoiceHandle note) { return m_mixer.releaseNote(note); }
        bool stopNote(VoiceHandle note) { return m_mixer.stopNote(note); }
        bool setDelay(const DelaySettings& settings) { return m_mixer.setDelay(settings); }
        void setBusVolume(AudioBus bus, float volume) { m_mixer.setBusVolume(bus, volume); }
        // Music (#49): register a song (mge::Song) and play it in a loop; with fade, crossfades
        SongId addSong(const Song& song) { return m_mixer.addSong(song); }
        bool playSong(SongId song, float fadeSeconds = 0.0f) { return m_mixer.playSong(song, fadeSeconds); }
        bool stopMusic(float fadeSeconds = 0.0f) { return m_mixer.stopMusic(fadeSeconds); }
        bool stopAll() { return m_mixer.stopAll(); }

        void setMasterVolume(float volume) { m_mixer.setMasterVolume(volume); }
        float masterVolume() const { return m_mixer.masterVolume(); }
        void setMuted(bool muted) { m_mixer.setMuted(muted); }
        bool muted() const { return m_mixer.muted(); }
        MixerStats stats() const { return m_mixer.stats(); }

    private:
        static void feed(void* userdata, SDL_AudioStream* stream, int additionalBytes, int totalBytes);

        Mixer m_mixer;
        SDL_AudioStream* m_stream = nullptr;
        bool m_subsystem = false;
        std::vector<float> m_scratch; // Mixing block (preallocated in onInit, never on the audio thread)
    };
}
