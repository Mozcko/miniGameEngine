#pragma once

// Medidas simples sobre señales para los tests de audio (frecuencia, nivel)
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace synth_test {

    // Cruces por cero subiendo → Hz
    inline float frequency(const std::vector<float>& mono, int sampleRate) {
        int crossings = 0;
        for (std::size_t i = 1; i < mono.size(); i++)
            if (mono[i - 1] < 0.0f && mono[i] >= 0.0f) crossings++;
        return float(crossings) * float(sampleRate) / float(mono.size());
    }

    inline float rms(const std::vector<float>& mono, std::size_t from = 0, std::size_t to = 0) {
        if (to == 0 || to > mono.size()) to = mono.size();
        double sum = 0.0;
        for (std::size_t i = from; i < to; i++) sum += double(mono[i]) * mono[i];
        return to > from ? float(std::sqrt(sum / double(to - from))) : 0.0f;
    }

    inline float peak(const std::vector<float>& mono, std::size_t from = 0, std::size_t to = 0) {
        if (to == 0 || to > mono.size()) to = mono.size();
        float p = 0.0f;
        for (std::size_t i = from; i < to; i++) p = std::max(p, std::abs(mono[i]));
        return p;
    }

    // Canal izquierdo de un buffer estéreo intercalado
    inline std::vector<float> left(const std::vector<float>& stereo) {
        std::vector<float> out(stereo.size() / 2);
        for (std::size_t i = 0; i < out.size(); i++) out[i] = stereo[i * 2];
        return out;
    }
}
