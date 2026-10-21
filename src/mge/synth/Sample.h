#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace mge {

    // Formato del audio del motor: float32 en [-1, 1], estéreo intercalado (L, R, L, R…)
    constexpr int kAudioChannels = 2;
    constexpr int kDefaultSampleRate = 48000;

    // Un sonido ya en memoria (de un WAV o generado por código), a la frecuencia del mixer.
    // 1 canal (mono) o 2 (estéreo intercalado)
    struct Sample {
        std::vector<float> data;
        int channels = 1;

        std::size_t frameCount() const { return channels > 0 ? data.size() / std::size_t(channels) : 0; }
        bool empty() const { return data.empty(); }
    };

    using SampleId = std::uint32_t;
    constexpr SampleId kInvalidSample = 0xFFFFFFFFu;
}
