#include <mge/audio/AudioModule.h>

#include <mge/core/Assets.h>
#include <mge/core/Log.h>

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <string>

namespace mge {

    namespace {
        constexpr std::size_t kBlockFrames = 1024; // Frames por llamada a Mixer::render

        // Libera el subsistema de audio; si era el último de SDL, cierra SDL por completo
        void quitAudioSubsystem() {
            SDL_QuitSubSystem(SDL_INIT_AUDIO);
            if (SDL_WasInit(0) == 0) SDL_Quit();
        }
    }

    AudioModule::AudioModule(int sampleRate) : m_mixer(sampleRate) {}

    AudioModule::~AudioModule() {
        // Por si el módulo se destruye sin onShutdown (init fallido de otro módulo)
        if (m_stream) SDL_DestroyAudioStream(m_stream);
    }

    bool AudioModule::onInit(Application& /*app*/) {
        if (const char* mute = std::getenv("MGE_MUTE"); mute && std::string(mute) == "1")
            m_mixer.setMuted(true);
        if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
            MGE_LOG_ERROR("Audio",
                          "No se pudo iniciar el audio de SDL (el juego sigue sin sonido): ", SDL_GetError());
            return true;
        }
        m_subsystem = true;

        m_scratch.assign(kBlockFrames * kAudioChannels, 0.0f);
        const SDL_AudioSpec spec{SDL_AUDIO_F32, kAudioChannels, m_mixer.sampleRate()};
        m_stream =
            SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, &AudioModule::feed, this);
        if (!m_stream) {
            MGE_LOG_ERROR("Audio", "No se pudo abrir el dispositivo de audio (el juego sigue sin sonido): ",
                          SDL_GetError());
            return true;
        }
        SDL_ResumeAudioStreamDevice(m_stream); // Los streams de dispositivo arrancan en pausa
        MGE_LOG_INFO("Audio", "Audio: ", SDL_GetCurrentAudioDriver(), ", ", m_mixer.sampleRate(),
                     " Hz estéreo float32", m_mixer.muted() ? " (silenciado: MGE_MUTE=1)" : "");
        return true;
    }

    void AudioModule::onShutdown(Application& /*app*/) {
        if (m_stream) {
            SDL_DestroyAudioStream(m_stream); // Cierra el dispositivo; el callback ya no corre
            m_stream = nullptr;
        }
        if (m_subsystem) {
            quitAudioSubsystem();
            m_subsystem = false;
            MGE_LOG_INFO("Audio", "Audio cerrado");
        }
    }

    // Hilo de audio de SDL: pide additionalBytes más. Se mezcla en bloques en m_scratch (ya reservado)
    void AudioModule::feed(void* userdata, SDL_AudioStream* stream, int additionalBytes, int /*totalBytes*/) {
        auto* self = static_cast<AudioModule*>(userdata);
        constexpr int kFrameBytes = int(sizeof(float)) * kAudioChannels;
        std::size_t frames = std::size_t(std::max(additionalBytes, 0) / kFrameBytes);
        while (frames > 0) {
            const std::size_t block = std::min(frames, kBlockFrames);
            self->m_mixer.render(self->m_scratch.data(), block);
            SDL_PutAudioStreamData(stream, self->m_scratch.data(), int(block) * kFrameBytes);
            frames -= block;
        }
    }

    SampleId AudioModule::loadWav(const std::filesystem::path& path) {
        const std::filesystem::path full = path.is_absolute() ? path : assetPath(path.generic_string());
        SDL_AudioSpec source{};
        Uint8* data = nullptr;
        Uint32 length = 0;
        if (!SDL_LoadWAV(full.string().c_str(), &source, &data, &length)) {
            MGE_LOG_ERROR("Audio", "No se pudo cargar ", full.string(), ": ", SDL_GetError());
            return kInvalidSample;
        }
        // A float32 a la frecuencia del mezclador; mono se queda mono (el mezclador lo pone en los dos lados)
        const int channels = std::min(source.channels, kAudioChannels);
        const SDL_AudioSpec target{SDL_AUDIO_F32, channels, m_mixer.sampleRate()};
        Uint8* converted = nullptr;
        int convertedLength = 0;
        const bool ok =
            SDL_ConvertAudioSamples(&source, data, int(length), &target, &converted, &convertedLength);
        SDL_free(data);
        if (!ok) {
            MGE_LOG_ERROR("Audio", "No se pudo convertir ", full.string(), ": ", SDL_GetError());
            return kInvalidSample;
        }
        Sample sample;
        sample.channels = channels;
        sample.data.resize(std::size_t(convertedLength) / sizeof(float));
        std::memcpy(sample.data.data(), converted, sample.data.size() * sizeof(float));
        SDL_free(converted);
        const SampleId id = m_mixer.addSample(std::move(sample));
        if (id == kInvalidSample)
            MGE_LOG_ERROR("Audio", "No entran más sonidos en el banco: ", full.string());
        return id;
    }
}
