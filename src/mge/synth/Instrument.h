#pragma once

#include <mge/synth/Envelope.h>
#include <mge/synth/Filter.h>
#include <mge/synth/Oscillator.h>

namespace mge {

    // How a note sounds (#47): oscillator + envelope + filter, with vibrato, pitch slide, and delay
    // send. Plain data (copied into mixer commands): defining instruments is just C++ code
    struct Instrument {
        Waveform wave = Waveform::Square;
        float pulseWidth = 0.5f;
        Adsr envelope;
        FilterMode filter = FilterMode::None;
        float cutoff = 4000.0f; // Hz
        float resonance = 0.0f; // 0..1
        float gain = 0.5f;
        float pan = 0.0f;
        float vibratoDepth = 0.0f; // Semitones
        float vibratoRate = 5.0f;  // Hz
        float pitchSlide = 0.0f;   // Semitones per second (negative = down)
        float delaySend = 0.0f;    // 0..1 send level to the mixer's echo
    };
}
