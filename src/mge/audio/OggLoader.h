#pragma once

#include <mge/synth/Sample.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace mge {

    // Optional OGG Vorbis sound-effect decoding (#235), using stb_vorbis (non-streamed).
    // Decodes the entire stream into memory as float32 PCM at sampleRate (mono stays mono;
    // stereo or multi-channel audio is converted to kAudioChannels).
    // On failure, an error is logged and an empty Sample is returned.
    Sample loadOgg(const std::uint8_t* oggBytes, std::size_t size, int sampleRate = kDefaultSampleRate);
    Sample loadOgg(const std::vector<std::uint8_t>& oggBytes, int sampleRate = kDefaultSampleRate);
    // Relative path -> resolved via mge::assetPath (assets/)
    Sample loadOggFile(const std::filesystem::path& path, int sampleRate = kDefaultSampleRate);
}
