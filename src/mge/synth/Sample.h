#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace mge {

    // Engine audio format: float32 in [-1, 1], interleaved stereo (L, R, L, R…)
    constexpr int kAudioChannels = 2;
    constexpr int kDefaultSampleRate = 48000;

    // A sound already in memory (from a WAV or procedurally generated), at the mixer's sample rate.
    // 1 channel (mono) or 2 (interleaved stereo)
    struct Sample {
        std::vector<float> data;
        int channels = 1;

        std::size_t frameCount() const { return channels > 0 ? data.size() / std::size_t(channels) : 0; }
        bool empty() const { return data.empty(); }
    };

    using SampleId = std::uint32_t;
    constexpr SampleId kInvalidSample = 0xFFFFFFFFu;
}
