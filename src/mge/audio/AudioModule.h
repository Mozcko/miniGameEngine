#pragma once

#include <mge/core/Module.h>
#include <mge/synth/Mixer.h>

#include <filesystem>
#include <vector>

struct SDL_AudioStream;

namespace mge {

    // Módulo de audio (#46): conecta un Mixer (mge_synth) al dispositivo de audio por defecto con un
    // SDL_AudioStream en float32 estéreo. SDL llama al mezclador desde su hilo de audio; el juego le habla al
    // mezclador por una cola sin locks. Sin dispositivo (o con SDL_AUDIODRIVER=dummy) el juego sigue en
    // silencio: no es un error. MGE_MUTE=1 arranca en silencio.
    //   auto* audio = getModule<mge::AudioModule>();
    //   audio->playTone({440.0f, 0.3f});                   // sin archivos
    //   const SampleId jump = audio->loadWav("sounds/jump.wav");
    //   audio->play(jump);
    // Hilo del juego: un solo hilo llama a play/load/volumen.
    class AudioModule : public Module {
    public:
        explicit AudioModule(int sampleRate = kDefaultSampleRate);
        ~AudioModule() override;

        const char* name() const override { return "Audio"; }
        bool onInit(Application& app) override;
        void onShutdown(Application& app) override;

        // ¿Hay un dispositivo sonando? (false: todo sigue funcionando, pero no se oye)
        bool hasDevice() const { return m_stream != nullptr; }
        Mixer& mixer() { return m_mixer; }

        // WAV → Sample a la frecuencia del mezclador (relativo → mge::assetPath). kInvalidSample si falla
        SampleId loadWav(const std::filesystem::path& path);
        SampleId addSample(Sample sample) { return m_mixer.addSample(std::move(sample)); }

        bool play(SampleId sample, float gain = 1.0f, float pan = 0.0f) {
            return m_mixer.playSample(sample, gain, pan);
        }
        bool playTone(const ToneDesc& tone) { return m_mixer.playTone(tone); }
        // Una nota de un instrumento sintetizado (#47); ver Mixer::playNote
        VoiceHandle playNote(const Instrument& instrument, float frequency, float velocity = 1.0f,
                             float duration = 0.25f, AudioBus bus = AudioBus::Effects) {
            return m_mixer.playNote(instrument, frequency, velocity, duration, bus);
        }
        bool releaseNote(VoiceHandle note) { return m_mixer.releaseNote(note); }
        bool stopNote(VoiceHandle note) { return m_mixer.stopNote(note); }
        bool setDelay(const DelaySettings& settings) { return m_mixer.setDelay(settings); }
        void setBusVolume(AudioBus bus, float volume) { m_mixer.setBusVolume(bus, volume); }
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
        std::vector<float> m_scratch; // Bloque de mezcla (se reserva en onInit, nunca en el hilo de audio)
    };
}
