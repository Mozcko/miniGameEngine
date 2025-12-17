#include "AudioManager.h"
#include <iostream>

namespace MiniGameEngine {

    // Inicializar mapas estáticos
    std::map<std::string, Mix_Chunk*> AudioManager::_sfxCache;
    std::map<std::string, Mix_Music*> AudioManager::_musicCache;

    void AudioManager::init() {
        // Inicializar SDL_Mixer
        // Frecuencia: 44100Hz, Formato: Default, Canales: 2 (Stereo), ChunkSize: 2048
        if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
            std::cerr << "[AudioManager] Error fatal al iniciar SDL_Mixer: " << Mix_GetError() << std::endl;
        } else {
            std::cout << "[AudioManager] Sistema de audio inicializado." << std::endl;
        }
    }

    void AudioManager::shutdown() {
        // Liberar efectos de sonido
        for (auto& pair : _sfxCache) {
            Mix_FreeChunk(pair.second);
        }
        _sfxCache.clear();

        // Liberar música
        for (auto& pair : _musicCache) {
            Mix_FreeMusic(pair.second);
        }
        _musicCache.clear();

        Mix_CloseAudio();
        std::cout << "[AudioManager] Sistema de audio cerrado." << std::endl;
    }

    void AudioManager::playSound(const std::string& filePath, int volume) {
        Mix_Chunk* chunk = nullptr;

        // 1. Buscar en Caché
        auto it = _sfxCache.find(filePath);
        if (it != _sfxCache.end()) {
            chunk = it->second;
        } else {
            // 2. Cargar si no existe
            chunk = Mix_LoadWAV(filePath.c_str());
            if (!chunk) {
                std::cerr << "[AudioManager] No se pudo cargar SFX: " << filePath << " Error: " << Mix_GetError() << std::endl;
                return;
            }
            _sfxCache[filePath] = chunk;
        }

        // 3. Configurar volumen y reproducir
        Mix_VolumeChunk(chunk, volume); // 0 a 128
        
        // Canal -1 = Elige el primer canal libre disponible
        if (Mix_PlayChannel(-1, chunk, 0) == -1) {
            std::cerr << "[AudioManager] No hay canales libres para reproducir sonido." << std::endl;
        }
    }

    void AudioManager::playMusic(const std::string& filePath, int loops) {
        Mix_Music* music = nullptr;

        // 1. Buscar en Caché
        auto it = _musicCache.find(filePath);
        if (it != _musicCache.end()) {
            music = it->second;
        } else {
            // 2. Cargar si no existe
            music = Mix_LoadMUS(filePath.c_str());
            if (!music) {
                std::cerr << "[AudioManager] No se pudo cargar Musica: " << filePath << " Error: " << Mix_GetError() << std::endl;
                return;
            }
            _musicCache[filePath] = music;
        }

        // 3. Reproducir
        if (Mix_PlayMusic(music, loops) == -1) {
            std::cerr << "[AudioManager] Error reproduciendo música: " << Mix_GetError() << std::endl;
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