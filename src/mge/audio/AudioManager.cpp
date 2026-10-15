#include <mge/audio/AudioManager.h>
#include <mge/core/Log.h>

namespace mge {

    // Inicializar mapas estáticos
    std::map<std::string, Mix_Chunk*> AudioManager::s_sfxCache;
    std::map<std::string, Mix_Music*> AudioManager::s_musicCache;

    void AudioManager::init() {
        // Inicializar SDL_Mixer
        // Frecuencia: 44100Hz, Formato: Default, Canales: 2 (Stereo), ChunkSize: 2048
        if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
            MGE_LOG_ERROR("Audio",
                          "No se pudo iniciar SDL_mixer (el juego sigue sin sonido): ", Mix_GetError());
        } else {
            MGE_LOG_INFO("Audio", "Sistema de audio inicializado");
        }
    }

    void AudioManager::shutdown() {
        // Liberar efectos de sonido
        for (auto& pair : s_sfxCache) {
            Mix_FreeChunk(pair.second);
        }
        s_sfxCache.clear();

        // Liberar música
        for (auto& pair : s_musicCache) {
            Mix_FreeMusic(pair.second);
        }
        s_musicCache.clear();

        Mix_CloseAudio();
        MGE_LOG_INFO("Audio", "Sistema de audio cerrado");
    }

    void AudioManager::playSound(const std::string& filePath, int volume) {
        Mix_Chunk* chunk = nullptr;

        // 1. Buscar en Caché
        auto it = s_sfxCache.find(filePath);
        if (it != s_sfxCache.end()) {
            chunk = it->second;
        } else {
            // 2. Cargar si no existe
            chunk = Mix_LoadWAV(filePath.c_str());
            if (!chunk) {
                MGE_LOG_ERROR("Audio", "No se pudo cargar el SFX ", filePath, ": ", Mix_GetError());
                return;
            }
            s_sfxCache[filePath] = chunk;
        }

        // 3. Configurar volumen y reproducir
        Mix_VolumeChunk(chunk, volume); // 0 a 128

        // Canal -1 = Elige el primer canal libre disponible
        if (Mix_PlayChannel(-1, chunk, 0) == -1) {
            MGE_LOG_WARN("Audio", "No hay canales libres para reproducir el sonido");
        }
    }

    void AudioManager::playMusic(const std::string& filePath, int loops) {
        Mix_Music* music = nullptr;

        // 1. Buscar en Caché
        auto it = s_musicCache.find(filePath);
        if (it != s_musicCache.end()) {
            music = it->second;
        } else {
            // 2. Cargar si no existe
            music = Mix_LoadMUS(filePath.c_str());
            if (!music) {
                MGE_LOG_ERROR("Audio", "No se pudo cargar la música ", filePath, ": ", Mix_GetError());
                return;
            }
            s_musicCache[filePath] = music;
        }

        // 3. Reproducir
        if (Mix_PlayMusic(music, loops) == -1) {
            MGE_LOG_ERROR("Audio", "Error reproduciendo música: ", Mix_GetError());
        }
    }

    void AudioManager::pauseMusic() {
        if (Mix_PlayingMusic()) Mix_PauseMusic();
    }

    void AudioManager::resumeMusic() {
        if (Mix_PausedMusic()) Mix_ResumeMusic();
    }

    void AudioManager::stopMusic() {
        Mix_HaltMusic();
    }
}