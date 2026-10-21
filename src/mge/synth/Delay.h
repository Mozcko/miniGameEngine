#pragma once

#include <cstddef>
#include <vector>

namespace mge {

    struct DelaySettings {
        float time = 0.3f;      // Segundos (hasta el máximo del Delay)
        float feedback = 0.35f; // 0..0.95
        float mix = 0.0f;       // Cuánto de la línea se suma a la salida (0 = apagado)
    };

    // Eco estéreo (#47) con una línea de largo máximo fija: la memoria se reserva al construirlo (hilo del
    // juego); process() no reserva nada. El mezclador tiene uno como envío ("send")
    class Delay {
    public:
        Delay(float maxSeconds, int sampleRate);

        void setSettings(const DelaySettings& settings);
        const DelaySettings& settings() const { return m_settings; }
        // Entra (inLeft, inRight); devuelve la parte con eco a sumar (ya × mix)
        void process(float inLeft, float inRight, float& outLeft, float& outRight);
        void clear();

    private:
        std::vector<float> m_buffer; // Estéreo intercalado
        std::size_t m_frames = 0;
        std::size_t m_write = 0;
        std::size_t m_delayFrames = 1;
        int m_sampleRate;
        DelaySettings m_settings;
    };
}
