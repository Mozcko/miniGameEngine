#pragma once

#include <mge/synth/Mixer.h>

#include <cstddef>
#include <filesystem>
#include <vector>

namespace mge {

    // Audio sin dispositivo (#50): el mezclador corre en el hilo que llama y el resultado queda en memoria.
    // Para tests, la galería y exportar sonidos generados. Estéreo intercalado, a mixer.sampleRate()
    std::vector<float> renderOffline(Mixer& mixer, std::size_t frames);
    std::vector<float> renderOffline(Mixer& mixer, float seconds);

    // WAV PCM de 16 bits sin dependencias (como writePng). samples: channels intercalados en [-1, 1]
    bool writeWav(const std::filesystem::path& path, const std::vector<float>& samples, int channels,
                  int sampleRate);
    bool writeWav(const std::filesystem::path& path, const Sample& sample, int sampleRate);

    // Medidas de una señal mono, para tests y herramientas
    namespace analysis {
        // Un canal de un buffer intercalado
        std::vector<float> channel(const std::vector<float>& interleaved, int channels, int index);
        float rms(const std::vector<float>& mono, std::size_t from = 0, std::size_t to = 0);
        float peak(const std::vector<float>& mono, std::size_t from = 0, std::size_t to = 0);
        // Frecuencia por cruces por cero subiendo (señales con un tono dominante)
        float zeroCrossingHz(const std::vector<float>& mono, int sampleRate, std::size_t from = 0,
                             std::size_t to = 0);
        // Amplitud de la componente en hz (Goertzel) dentro de [from, to)
        float toneAmplitude(const std::vector<float>& mono, float hz, int sampleRate, std::size_t from = 0,
                            std::size_t to = 0);
        // Centroide espectral (Hz) de una ventana de 2048 muestras desde from: "brillo" del sonido
        float spectralCentroid(const std::vector<float>& mono, int sampleRate, std::size_t from = 0);
        // Muestras donde el nivel (RMS en ventanas de 5 ms) cruza threshold subiendo: comienzos de notas
        std::vector<std::size_t> onsets(const std::vector<float>& mono, int sampleRate, float threshold);
        // Fin del sonido: última muestra con |x| > threshold (0 si nunca)
        std::size_t lastAbove(const std::vector<float>& mono, float threshold);
    }
}
