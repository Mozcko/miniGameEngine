#pragma once

#include <mge/synth/Envelope.h>
#include <mge/synth/Filter.h>
#include <mge/synth/Oscillator.h>

namespace mge {

    // Cómo suena una nota (#47): oscilador + envolvente + filtro, con vibrato, deslizamiento de tono y envío
    // al eco. Es un dato plano (se copia en los comandos del mezclador): definir instrumentos es código C++
    struct Instrument {
        Waveform wave = Waveform::Square;
        float pulseWidth = 0.5f;
        Adsr envelope;
        FilterMode filter = FilterMode::None;
        float cutoff = 4000.0f; // Hz
        float resonance = 0.0f; // 0..1
        float gain = 0.5f;
        float pan = 0.0f;
        float vibratoDepth = 0.0f; // Semitonos
        float vibratoRate = 5.0f;  // Hz
        float pitchSlide = 0.0f;   // Semitonos por segundo (negativo = baja)
        float delaySend = 0.0f;    // 0..1 hacia el eco del mezclador
    };
}
