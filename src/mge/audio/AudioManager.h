#pragma once

#include <SDL3/SDL.h>
#include <map>
#include <string>
#include <vector>

namespace mge {

    // Audio sobre SDL3 (sin SDL_mixer): efectos WAV reproducidos con SDL_AudioStream.
    // La música y la síntesis procedural llegan en M5 (#46-#49).
    class AudioManager {
    public:
        // Abre el dispositivo de audio por defecto. Si falla, el juego sigue sin sonido.
        static void init();

        // Libera sonidos, streams y el dispositivo
        static void shutdown();

        // Reproduce un efecto WAV (fire and forget). volume: 0..128 (128 = original)
        static void playSound(const std::string& filePath, int volume = 128);

        // Música: no soportada hasta el secuenciador procedural (#49). Solo deja un aviso en el log.
        static void playMusic(const std::string& filePath, int loops = -1);
        static void pauseMusic();
        static void resumeMusic();
        static void stopMusic();

    private:
        // Un WAV cargado en memoria
        struct Sound {
            SDL_AudioSpec spec{};
            Uint8* data = nullptr;
            Uint32 length = 0;
        };

        // Libera los streams que ya terminaron de sonar
        static void releaseFinishedStreams();

        static SDL_AudioDeviceID s_device;
        // Caché para no cargar el mismo archivo dos veces
        static std::map<std::string, Sound> s_soundCache;
        // Un stream por sonido en reproducción
        static std::vector<SDL_AudioStream*> s_activeStreams;
    };

}
