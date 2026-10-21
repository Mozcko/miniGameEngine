#pragma once

#include <mge/synth/Oscillator.h>
#include <mge/synth/Sample.h>

#include <cstdint>

namespace mge {

    // Parámetros de un efecto de sonido al estilo sfxr (#48). Un efecto es un dato: se define en código, se
    // genera una vez con renderSfx y suena como cualquier sample (tan barato como un WAV).
    struct SfxParams {
        Waveform wave = Waveform::Square;
        float duty = 0.5f;      // Ancho de pulso (cuadrada)
        float dutySweep = 0.0f; // Cambio del ancho por segundo

        float baseFrequency = 440.0f;    // Hz al empezar
        float minFrequency = 20.0f;      // Si el deslizamiento baja de acá, el sonido termina
        float slide = 0.0f;              // Octavas por segundo (+ sube, − baja)
        float deltaSlide = 0.0f;         // Cambio del deslizamiento, octavas/s²
        float vibratoDepth = 0.0f;       // Semitonos
        float vibratoSpeed = 0.0f;       // Hz
        float arpeggioMultiplier = 1.0f; // La frecuencia se multiplica por esto tras arpeggioTime (1 = no)
        float arpeggioTime = 0.0f;       // Segundos
        float repeatTime = 0.0f;         // Cada tanto vuelve a empezar tono y arpegio (0 = no)

        // Envolvente: sube en attack, se mantiene sustain (con punch extra al principio), baja en decay
        float attack = 0.0f;
        float sustain = 0.1f;
        float punch = 0.0f; // 0..1
        float decay = 0.2f;

        float lowPassCutoff = 0.0f; // Hz (0 = sin filtro)
        float lowPassSweep = 0.0f;  // Octavas por segundo
        float lowPassResonance = 0.0f;
        float highPassCutoff = 0.0f; // Hz (0 = sin filtro)
        float highPassSweep = 0.0f;  // Octavas por segundo
        float phaserOffset = 0.0f;   // Milisegundos (0 = sin phaser)
        float phaserSweep = 0.0f;    // Milisegundos por segundo

        float gain = 0.5f;

        float duration() const { return attack + sustain + decay; }
    };

    // Genera el efecto (mono) a sampleRate. Determinista: misma entrada y semilla → mismas muestras.
    // Corre en el hilo del juego (reserva memoria): registrar el resultado con addSample/AudioModule::addSfx
    Sample renderSfx(const SfxParams& params, int sampleRate = kDefaultSampleRate, std::uint64_t seed = 0);

    // Efectos listos, equivalentes a los WAV de assets/sounds. Con seed != 0 salen variaciones leves
    // (mutate), útiles para que no suene siempre igual
    namespace sfx {
        SfxParams jump(std::uint64_t seed = 0);
        SfxParams coin(std::uint64_t seed = 0);
        SfxParams hurt(std::uint64_t seed = 0);
        SfxParams explosion(std::uint64_t seed = 0);
        SfxParams powerUp(std::uint64_t seed = 0);
        SfxParams click(std::uint64_t seed = 0);

        // Un efecto al azar (con valores razonables) y una variación de otro: amount 0..1
        SfxParams randomize(std::uint64_t seed);
        SfxParams mutate(const SfxParams& params, float amount, std::uint64_t seed);
    }
}
