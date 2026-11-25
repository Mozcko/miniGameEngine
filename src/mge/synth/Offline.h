#pragma once

#include <mge/synth/Mixer.h>

#include <cstddef>
#include <filesystem>
#include <vector>

namespace mge {

    // Device-free audio (#50): runs the mixer on the calling thread and returns the output in memory.
    // For tests, the gallery, and exporting generated sounds. Interleaved stereo at mixer.sampleRate()
    std::vector<float> renderOffline(Mixer& mixer, std::size_t frames);
    std::vector<float> renderOffline(Mixer& mixer, float seconds);

    // Dependency-free 16-bit PCM WAV writer (like writePng). samples: interleaved channels in [-1, 1]
    bool writeWav(const std::filesystem::path& path, const std::vector<float>& samples, int channels,
                  int sampleRate);
    bool writeWav(const std::filesystem::path& path, const Sample& sample, int sampleRate);

    // Measurements on a mono signal, for tests and tools
    namespace analysis {
        // Extracts one channel from an interleaved buffer
        std::vector<float> channel(const std::vector<float>& interleaved, int channels, int index);
        float rms(const std::vector<float>& mono, std::size_t from = 0, std::size_t to = 0);
        float peak(const std::vector<float>& mono, std::size_t from = 0, std::size_t to = 0);
        // Frequency via rising zero crossings (signals with one dominant tone)
        float zeroCrossingHz(const std::vector<float>& mono, int sampleRate, std::size_t from = 0,
                             std::size_t to = 0);
        // Amplitude of the component at hz (Goertzel) within [from, to)
        float toneAmplitude(const std::vector<float>& mono, float hz, int sampleRate, std::size_t from = 0,
                            std::size_t to = 0);
        // Spectral centroid (Hz) over a 2048-sample window starting at from: "brightness" of the sound
        float spectralCentroid(const std::vector<float>& mono, int sampleRate, std::size_t from = 0);
        // Sample indices where level (5 ms RMS windows) crosses threshold going up: note starts
        std::vector<std::size_t> onsets(const std::vector<float>& mono, int sampleRate, float threshold);
        // End of the sound: last sample with |x| > threshold (0 if never)
        std::size_t lastAbove(const std::vector<float>& mono, float threshold);
    }
}
