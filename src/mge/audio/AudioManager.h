#pragma once

#include <string>
#include <map>
#include <SDL2/SDL_mixer.h>

namespace mge {

    class AudioManager {
    public:
        // Inicializa SDL_Mixer
        static void init();

        // Limpia memoria y cierra el sistema
        static void shutdown();

        // Reproduce efectos de sonido (WAV) - Fire and Forget
        static void playSound(const std::string& filePath, int volume = 128);

        // Reproduce música de fondo (MP3/OGG)
        // loops: -1 para infinito, 0 para una vez
        static void playMusic(const std::string& filePath, int loops = -1);

        // Control de música
        static void pauseMusic();
        static void resumeMusic();
        static void stopMusic();

    private:
        // Cachés para no cargar el mismo archivo dos veces
        static std::map<std::string, Mix_Chunk*> s_sfxCache;
        static std::map<std::string, Mix_Music*> s_musicCache;
    };

}