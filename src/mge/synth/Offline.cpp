#include <mge/synth/Offline.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>

namespace mge {

    std::vector<float> renderOffline(Mixer& mixer, std::size_t frames) {
        std::vector<float> out(frames * kAudioChannels);
        mixer.render(out.data(), frames);
        return out;
    }

    std::vector<float> renderOffline(Mixer& mixer, float seconds) {
        return renderOffline(mixer, std::size_t(std::max(seconds, 0.0f) * float(mixer.sampleRate())));
    }

    namespace {
        void put16(std::ofstream& out, std::uint16_t v) {
            const char bytes[2] = {char(v & 0xFF), char(v >> 8)};
            out.write(bytes, 2);
        }
        void put32(std::ofstream& out, std::uint32_t v) {
            const char bytes[4] = {char(v & 0xFF), char((v >> 8) & 0xFF), char((v >> 16) & 0xFF),
                                   char(v >> 24)};
            out.write(bytes, 4);
        }
    }

    bool writeWav(const std::filesystem::path& path, const std::vector<float>& samples, int channels,
                  int sampleRate) {
        if (channels < 1 || sampleRate <= 0) return false;
        std::ofstream out(path, std::ios::binary);
        if (!out) return false;
        const auto dataBytes = std::uint32_t(samples.size() * 2);
        out.write("RIFF", 4);
        put32(out, 36 + dataBytes);
        out.write("WAVEfmt ", 8);
        put32(out, 16);
        put16(out, 1); // PCM
        put16(out, std::uint16_t(channels));
        put32(out, std::uint32_t(sampleRate));
        put32(out, std::uint32_t(sampleRate * channels * 2));
        put16(out, std::uint16_t(channels * 2));
        put16(out, 16);
        out.write("data", 4);
        put32(out, dataBytes);
        for (float s : samples) {
            const auto value = std::int16_t(std::lround(std::clamp(s, -1.0f, 1.0f) * 32767.0f));
            put16(out, std::uint16_t(value));
        }
        return bool(out);
    }

    bool writeWav(const std::filesystem::path& path, const Sample& sample, int sampleRate) {
        return writeWav(path, sample.data, sample.channels, sampleRate);
    }

    namespace analysis {
        namespace {
            std::size_t end(const std::vector<float>& mono, std::size_t to) {
                return to == 0 || to > mono.size() ? mono.size() : to;
            }
        }

        std::vector<float> channel(const std::vector<float>& interleaved, int channels, int index) {
            std::vector<float> out(interleaved.size() / std::size_t(channels));
            for (std::size_t i = 0; i < out.size(); i++)
                out[i] = interleaved[i * std::size_t(channels) + std::size_t(index)];
            return out;
        }

        float rms(const std::vector<float>& mono, std::size_t from, std::size_t to) {
            to = end(mono, to);
            if (to <= from) return 0.0f;
            double sum = 0.0;
            for (std::size_t i = from; i < to; i++) sum += double(mono[i]) * mono[i];
            return float(std::sqrt(sum / double(to - from)));
        }

        float peak(const std::vector<float>& mono, std::size_t from, std::size_t to) {
            to = end(mono, to);
            float p = 0.0f;
            for (std::size_t i = from; i < to; i++) p = std::max(p, std::abs(mono[i]));
            return p;
        }

        float zeroCrossingHz(const std::vector<float>& mono, int sampleRate, std::size_t from,
                             std::size_t to) {
            to = end(mono, to);
            if (to <= from + 1) return 0.0f;
            int crossings = 0;
            for (std::size_t i = from + 1; i < to; i++)
                if (mono[i - 1] < 0.0f && mono[i] >= 0.0f) crossings++;
            return float(crossings) * float(sampleRate) / float(to - from);
        }

        float toneAmplitude(const std::vector<float>& mono, float hz, int sampleRate, std::size_t from,
                            std::size_t to) {
            to = end(mono, to);
            if (to <= from) return 0.0f;
            const double w = 2.0 * 3.141592653589793 * double(hz) / double(sampleRate);
            const double coeff = 2.0 * std::cos(w);
            double s1 = 0.0, s2 = 0.0;
            for (std::size_t i = from; i < to; i++) {
                const double s = double(mono[i]) + coeff * s1 - s2;
                s2 = s1;
                s1 = s;
            }
            const double power = s1 * s1 + s2 * s2 - coeff * s1 * s2;
            return float(2.0 * std::sqrt(std::max(power, 0.0)) / double(to - from));
        }

        float spectralCentroid(const std::vector<float>& mono, int sampleRate, std::size_t from) {
            // DFT con ventana de Hann sobre 2048 muestras (todas las bandas hasta Nyquist)
            constexpr std::size_t kWindow = 2048;
            if (from + kWindow > mono.size()) return 0.0f;
            constexpr double kPi = 3.141592653589793;
            std::vector<double> windowed(kWindow);
            for (std::size_t i = 0; i < kWindow; i++)
                windowed[i] = double(mono[from + i]) *
                              (0.5 - 0.5 * std::cos(2.0 * kPi * double(i) / double(kWindow - 1)));
            double weighted = 0.0, total = 0.0;
            for (std::size_t k = 1; k < kWindow / 2; k++) {
                const double coeff = 2.0 * std::cos(2.0 * kPi * double(k) / double(kWindow));
                double s1 = 0.0, s2 = 0.0;
                for (double x : windowed) {
                    const double s = x + coeff * s1 - s2;
                    s2 = s1;
                    s1 = s;
                }
                const double magnitude = std::sqrt(std::max(s1 * s1 + s2 * s2 - coeff * s1 * s2, 0.0));
                const double hz = double(k) * double(sampleRate) / double(kWindow);
                weighted += magnitude * hz;
                total += magnitude;
            }
            return total > 0.0 ? float(weighted / total) : 0.0f;
        }

        std::vector<std::size_t> onsets(const std::vector<float>& mono, int sampleRate, float threshold) {
            const std::size_t window = std::max<std::size_t>(1, std::size_t(sampleRate) / 200); // 5 ms
            std::vector<std::size_t> result;
            bool above = false;
            for (std::size_t start = 0; start + window <= mono.size(); start += window) {
                const bool now = rms(mono, start, start + window) > threshold;
                if (now && !above) result.push_back(start);
                above = now;
            }
            return result;
        }

        std::size_t lastAbove(const std::vector<float>& mono, float threshold) {
            for (std::size_t i = mono.size(); i-- > 0;)
                if (std::abs(mono[i]) > threshold) return i;
            return 0;
        }
    }
}
