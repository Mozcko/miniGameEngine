#include <doctest/doctest.h>
#include <mge/synth/Mixer.h>

#include <algorithm>
#include <cmath>
#include <thread>
#include <vector>

using namespace mge;

namespace {
    constexpr int kRate = 48000;

    std::vector<float> render(Mixer& mixer, std::size_t frames, std::size_t block = 512) {
        std::vector<float> out(frames * 2);
        for (std::size_t done = 0; done < frames; done += block)
            mixer.render(out.data() + done * 2, std::min(block, frames - done));
        return out;
    }

    // Channel (0 = left) between two frames
    float peak(const std::vector<float>& buffer, int channel, std::size_t from, std::size_t to) {
        float p = 0.0f;
        for (std::size_t i = from; i < to; i++)
            p = std::max(p, std::abs(buffer[i * 2 + std::size_t(channel)]));
        return p;
    }

    // Rising zero-crossings → frequency
    float frequency(const std::vector<float>& buffer, std::size_t from, std::size_t to) {
        int crossings = 0;
        for (std::size_t i = from + 1; i < to; i++)
            if (buffer[(i - 1) * 2] < 0.0f && buffer[i * 2] >= 0.0f) crossings++;
        return float(crossings) * float(kRate) / float(to - from);
    }
}

TEST_CASE("Mixer: a sine tone has the requested frequency, amplitude, and duration") {
    Mixer mixer(kRate);
    REQUIRE(mixer.playTone({440.0f, 0.5f, 0.5f, 0.0f}));
    const std::vector<float> out = render(mixer, kRate); // 1 s
    CHECK(frequency(out, 1000, 20000) == doctest::Approx(440.0f).epsilon(0.01));
    // Centered: 0.5 × cos(45°) per channel
    CHECK(peak(out, 0, 1000, 20000) == doctest::Approx(0.5f * 0.7071f).epsilon(0.01));
    CHECK(peak(out, 1, 1000, 20000) == doctest::Approx(0.5f * 0.7071f).epsilon(0.01));
    CHECK(peak(out, 0, 24100, kRate) == doctest::Approx(0.0f)); // Ended at 0.5 s
    CHECK(out[0] == doctest::Approx(0.0f));                     // Starts with a ramp (no click)
    CHECK(mixer.stats().activeVoices == 0);
}

TEST_CASE("Mixer: pan, master volume, and mute") {
    Mixer mixer(kRate);
    mixer.playTone({300.0f, 1.0f, 0.8f, -1.0f}); // Hard left
    std::vector<float> out = render(mixer, 4800);
    CHECK(peak(out, 0, 1000, 4800) > 0.7f);
    CHECK(peak(out, 1, 1000, 4800) < 1e-4f);

    mixer.setMasterVolume(0.25f);
    render(mixer, 512); // Ramp reaches the new value in one block
    out = render(mixer, 4800);
    CHECK(peak(out, 0, 0, 4800) == doctest::Approx(0.2f).epsilon(0.02));

    mixer.setMuted(true);
    render(mixer, 512);
    out = render(mixer, 4800);
    CHECK(peak(out, 0, 0, 4800) == doctest::Approx(0.0f));
    CHECK(mixer.muted());
    mixer.setMasterVolume(7.0f); // Clamped to 1
    CHECK(mixer.masterVolume() == doctest::Approx(1.0f));
}

TEST_CASE("Mixer: a sample plays once in full and releases its voice") {
    Mixer mixer(kRate);
    Sample ramp;
    ramp.channels = 1;
    for (int i = 0; i < 1000; i++) ramp.data.push_back(float(i) / 1000.0f);
    const SampleId id = mixer.addSample(ramp);
    REQUIRE(id != kInvalidSample);
    REQUIRE(mixer.sample(id) != nullptr);
    CHECK(mixer.sample(id)->frameCount() == 1000);
    CHECK(mixer.addSample(Sample{}) == kInvalidSample); // Empty
    CHECK_FALSE(mixer.playSample(id + 5));              // Unknown ID

    REQUIRE(mixer.playSample(id, 1.0f, 0.0f));
    const std::vector<float> out = render(mixer, 2000, 300); // Multiple blocks
    // Mono centered at gain 1 = sample as-is on both channels
    CHECK(out[500 * 2] == doctest::Approx(0.5f).epsilon(0.001));
    CHECK(out[500 * 2 + 1] == doctest::Approx(0.5f).epsilon(0.001));
    CHECK(out[700 * 2] == doctest::Approx(0.7f).epsilon(0.001)); // Below soft-clipping threshold (0.8)
    CHECK(out[1000 * 2] == doctest::Approx(0.0f));
    CHECK(mixer.stats().activeVoices == 0);
}

TEST_CASE("Mixer: stopAll cuts everything; full queue and excess voices break nothing") {
    Mixer mixer(kRate);
    for (int i = 0; i < 4; i++) mixer.playTone({200.0f + float(i) * 100.0f, 5.0f, 0.1f, 0.0f});
    render(mixer, 256);
    CHECK(mixer.stats().activeVoices == 4);
    mixer.stopAll();
    render(mixer, 256);
    CHECK(mixer.stats().activeVoices == 0);

    // More commands than queue capacity: dropped ones are counted
    std::size_t accepted = 0;
    for (std::size_t i = 0; i < Mixer::kCommandCapacity + 20; i++)
        accepted += mixer.playTone({500.0f, 1.0f, 0.01f}) ? 1 : 0;
    CHECK(accepted == Mixer::kCommandCapacity - 1);
    CHECK(mixer.stats().droppedCommands == 21);
    // More voices than pool size: oldest voices are stolen
    render(mixer, 256);
    CHECK(mixer.stats().activeVoices == Mixer::kMaxVoices);
    CHECK(mixer.stats().stolenVoices == accepted - Mixer::kMaxVoices);
    const std::vector<float> out = render(mixer, 256);
    CHECK(peak(out, 0, 0, 256) <= 1.0f); // Clamped to [-1, 1]
}

TEST_CASE("Mixer: game enqueues from another thread while audio renders") {
    Mixer mixer(kRate);
    std::atomic<bool> done{false};
    std::thread audio([&] {
        std::vector<float> block(256 * 2);
        while (!done.load()) mixer.render(block.data(), 256);
    });
    for (int i = 0; i < 2000; i++) {
        mixer.playTone({300.0f, 0.01f, 0.1f, 0.0f});
        if (i % 100 == 0) mixer.setMasterVolume(float(i % 7) / 7.0f);
    }
    done = true;
    audio.join();
    CHECK(mixer.stats().droppedCommands < 2000);
}
