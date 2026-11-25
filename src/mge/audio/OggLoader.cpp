#include <mge/audio/OggLoader.h>

#include <mge/core/Assets.h>
#include <mge/core/Log.h>

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
#include <utility>

// stb_vorbis isolated in this translation unit (non-streamed float32 decoding from memory only)
#define STB_VORBIS_NO_PUSHDATA_API
#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_NO_INTEGER_CONVERSION
#include <stb/stb_vorbis.c>

namespace mge {

    Sample loadOgg(const std::uint8_t* oggBytes, std::size_t size, int sampleRate) {
        if (!oggBytes || size == 0 || sampleRate <= 0) {
            MGE_LOG_ERROR("Audio", "Invalid OGG buffer or sample rate (", sampleRate, " Hz)");
            return {};
        }
        if (size > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
            MGE_LOG_ERROR("Audio", "OGG buffer is too large to decode");
            return {};
        }

        int error = VORBIS__no_error;
        stb_vorbis* vorbis = stb_vorbis_open_memory(oggBytes, static_cast<int>(size), &error, nullptr);
        if (!vorbis) {
            MGE_LOG_ERROR("Audio", "Failed to decode OGG Vorbis stream (error ", error, ")");
            return {};
        }

        const stb_vorbis_info info = stb_vorbis_get_info(vorbis);
        const int srcChannels = info.channels;
        const int srcSampleRate = static_cast<int>(info.sample_rate);
        if (srcChannels <= 0 || srcSampleRate <= 0) {
            stb_vorbis_close(vorbis);
            MGE_LOG_ERROR("Audio", "Invalid OGG Vorbis stream format (", srcChannels, " ch, ",
                          srcSampleRate, " Hz)");
            return {};
        }

        constexpr int kChunkFrames = 4096;
        std::vector<float> decoded;
        std::vector<float> chunk(static_cast<std::size_t>(kChunkFrames) *
                                 static_cast<std::size_t>(srcChannels));
        for (;;) {
            const int frames = stb_vorbis_get_samples_float_interleaved(
                vorbis, srcChannels, chunk.data(), static_cast<int>(chunk.size()));
            if (frames <= 0) break;
            decoded.insert(decoded.end(), chunk.begin(),
                           chunk.begin() + static_cast<std::ptrdiff_t>(frames) * srcChannels);
        }
        stb_vorbis_close(vorbis);

        if (decoded.empty()) {
            MGE_LOG_ERROR("Audio", "OGG Vorbis stream contains no audio samples");
            return {};
        }

        // Convert to float32 at sampleRate; mono stays mono (the mixer plays it on both channels)
        const int dstChannels = std::min(srcChannels, kAudioChannels);
        if (srcChannels == dstChannels && srcSampleRate == sampleRate) {
            Sample sample;
            sample.channels = dstChannels;
            sample.data = std::move(decoded);
            return sample;
        }

        const std::size_t srcBytes = decoded.size() * sizeof(float);
        if (srcBytes > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
            MGE_LOG_ERROR("Audio", "Decoded OGG stream is too large to resample");
            return {};
        }

        const SDL_AudioSpec source{SDL_AUDIO_F32, srcChannels, srcSampleRate};
        const SDL_AudioSpec target{SDL_AUDIO_F32, dstChannels, sampleRate};
        Uint8* converted = nullptr;
        int convertedLength = 0;
        const bool ok = SDL_ConvertAudioSamples(&source, reinterpret_cast<const Uint8*>(decoded.data()),
                                                static_cast<int>(srcBytes), &target, &converted,
                                                &convertedLength);
        if (!ok) {
            MGE_LOG_ERROR("Audio", "Could not convert OGG audio: ", SDL_GetError());
            return {};
        }

        Sample sample;
        sample.channels = dstChannels;
        sample.data.resize(static_cast<std::size_t>(convertedLength) / sizeof(float));
        std::memcpy(sample.data.data(), converted, sample.data.size() * sizeof(float));
        SDL_free(converted);
        return sample;
    }

    Sample loadOgg(const std::vector<std::uint8_t>& oggBytes, int sampleRate) {
        return loadOgg(oggBytes.data(), oggBytes.size(), sampleRate);
    }

    Sample loadOggFile(const std::filesystem::path& path, int sampleRate) {
        const std::filesystem::path full = path.is_absolute() ? path : assetPath(path.generic_string());
        std::ifstream file(full, std::ios::binary);
        if (!file) {
            MGE_LOG_ERROR("Audio", "Could not load ", full.string());
            return {};
        }
        const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)),
                                              std::istreambuf_iterator<char>());
        return loadOgg(bytes, sampleRate);
    }
}
