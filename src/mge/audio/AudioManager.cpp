#include <mge/audio/AudioManager.h>
#include <mge/core/Log.h>

#include <algorithm>

namespace mge {

    namespace {
        // Libera el subsistema de audio; si era el último de SDL, cierra SDL por completo
        void quitAudioSubsystem() {
            SDL_QuitSubSystem(SDL_INIT_AUDIO);
            if (SDL_WasInit(0) == 0) SDL_Quit();
        }
    }

    SDL_AudioDeviceID AudioManager::s_device = 0;
    std::map<std::string, AudioManager::Sound> AudioManager::s_soundCache;
    std::vector<SDL_AudioStream*> AudioManager::s_activeStreams;

    void AudioManager::init() {
        if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
            MGE_LOG_ERROR("Audio",
                          "No se pudo iniciar el audio de SDL (el juego sigue sin sonido): ", SDL_GetError());
            return;
        }

        s_device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
        if (s_device == 0) {
            MGE_LOG_ERROR("Audio", "No se pudo abrir el dispositivo de audio (el juego sigue sin sonido): ",
                          SDL_GetError());
            quitAudioSubsystem();
            return;
        }
        MGE_LOG_INFO("Audio", "Sistema de audio inicializado");
    }

    void AudioManager::shutdown() {
        for (SDL_AudioStream* stream : s_activeStreams) {
            SDL_DestroyAudioStream(stream);
        }
        s_activeStreams.clear();

        for (auto& pair : s_soundCache) {
            SDL_free(pair.second.data);
        }
        s_soundCache.clear();

        if (s_device != 0) {
            SDL_CloseAudioDevice(s_device);
            s_device = 0;
            quitAudioSubsystem();
            MGE_LOG_INFO("Audio", "Sistema de audio cerrado");
        }
    }

    void AudioManager::releaseFinishedStreams() {
        auto finished = [](SDL_AudioStream* stream) {
            if (SDL_GetAudioStreamQueued(stream) > 0) return false;
            SDL_DestroyAudioStream(stream);
            return true;
        };
        s_activeStreams.erase(std::remove_if(s_activeStreams.begin(), s_activeStreams.end(), finished),
                              s_activeStreams.end());
    }

    void AudioManager::playSound(const std::string& filePath, int volume) {
        if (s_device == 0) return; // Sin dispositivo: silencio

        releaseFinishedStreams();

        // 1. Buscar en caché o cargar el WAV
        auto it = s_soundCache.find(filePath);
        if (it == s_soundCache.end()) {
            Sound sound;
            if (!SDL_LoadWAV(filePath.c_str(), &sound.spec, &sound.data, &sound.length)) {
                MGE_LOG_ERROR("Audio", "No se pudo cargar el SFX ", filePath, ": ", SDL_GetError());
                return;
            }
            it = s_soundCache.emplace(filePath, sound).first;
        }
        const Sound& sound = it->second;

        // 2. Un stream convierte del formato del WAV al del dispositivo
        SDL_AudioSpec deviceSpec;
        if (!SDL_GetAudioDeviceFormat(s_device, &deviceSpec, nullptr)) {
            MGE_LOG_ERROR("Audio", "No se pudo leer el formato del dispositivo: ", SDL_GetError());
            return;
        }
        SDL_AudioStream* stream = SDL_CreateAudioStream(&sound.spec, &deviceSpec);
        if (!stream || !SDL_BindAudioStream(s_device, stream)) {
            MGE_LOG_ERROR("Audio", "No se pudo crear el stream de audio: ", SDL_GetError());
            if (stream) SDL_DestroyAudioStream(stream);
            return;
        }

        // 3. Volumen (0..128 como en SDL_mixer) y reproducir
        SDL_SetAudioStreamGain(stream, std::clamp(volume, 0, 128) / 128.0f);
        SDL_PutAudioStreamData(stream, sound.data, static_cast<int>(sound.length));
        SDL_FlushAudioStream(stream);
        s_activeStreams.push_back(stream);
    }

    void AudioManager::playMusic(const std::string& filePath, int /*loops*/) {
        MGE_LOG_WARN("Audio", "Música no soportada hasta el secuenciador procedural (#49); se ignora ",
                     filePath);
    }

    void AudioManager::pauseMusic() {}
    void AudioManager::resumeMusic() {}
    void AudioManager::stopMusic() {}
}
