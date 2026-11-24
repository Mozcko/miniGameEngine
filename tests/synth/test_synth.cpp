// Synthesis (#47): oscillators, envelopes, filters, delay, and mixer notes
#include "SynthChecks.h"

#include <doctest/doctest.h>
#include <mge/synth/Delay.h>
#include <mge/synth/Envelope.h>
#include <mge/synth/Filter.h>
#include <mge/synth/Mixer.h>
#include <mge/synth/Oscillator.h>

#include <vector>

using namespace mge;
using namespace synth_test;

namespace {
    constexpr int kRate = 48000;

    std::vector<float> run(Oscillator& osc, std::size_t n) {
        std::vector<float> out(n);
        for (float& v : out) v = osc.next();
        return out;
    }

    std::vector<float> renderMixer(Mixer& mixer, std::size_t frames) {
        std::vector<float> out(frames * 2);
        mixer.render(out.data(), frames);
        return out;
    }

    std::vector<float> sine(float hz, std::size_t n) {
        Oscillator osc;
        osc.setFrequency(hz, kRate);
        return run(osc, n);
    }
}

TEST_CASE("Oscillator: each waveform has the requested frequency and stays in [-1, 1]") {
    for (Waveform wave : {Waveform::Sine, Waveform::Square, Waveform::Saw, Waveform::Triangle}) {
        CAPTURE(int(wave));
        Oscillator osc;
        osc.setWaveform(wave);
        osc.setFrequency(220.0f, kRate);
        const std::vector<float> out = run(osc, kRate);
        CHECK(frequency(out, kRate) == doctest::Approx(220.0f).epsilon(0.01));
        CHECK(peak(out) <= 1.1f); // PolyBLEP can overshoot slightly at discontinuities
        CHECK(peak(out) >= 0.95f);
    }
    // Known RMS values: sine 1/√2, square 1, triangle 1/√3, saw 1/√3
    Oscillator osc;
    osc.setFrequency(100.0f, kRate);
    CHECK(rms(run(osc, kRate)) == doctest::Approx(0.7071f).epsilon(0.01));
    osc.setWaveform(Waveform::Triangle);
    CHECK(rms(run(osc, kRate)) == doctest::Approx(0.5774f).epsilon(0.01));
    osc.setWaveform(Waveform::Saw);
    CHECK(rms(run(osc, kRate)) == doctest::Approx(0.5774f).epsilon(0.02));
    osc.setWaveform(Waveform::Square);
    CHECK(rms(run(osc, kRate)) == doctest::Approx(1.0f).epsilon(0.02));
}

TEST_CASE("Oscillator: pulse width changes how long the square wave stays high") {
    Oscillator osc;
    osc.setWaveform(Waveform::Square);
    osc.setPulseWidth(0.25f);
    osc.setFrequency(100.0f, kRate);
    const std::vector<float> out = run(osc, kRate);
    std::size_t high = 0;
    for (float v : out) high += v > 0.0f ? 1 : 0;
    CHECK(float(high) / float(out.size()) == doctest::Approx(0.25f).epsilon(0.02));
}

TEST_CASE("Oscillator: deterministic seeded noise, zero-centered; 8-bit noise loops") {
    Oscillator a, b, c;
    for (Oscillator* osc : {&a, &b, &c}) {
        osc->setWaveform(Waveform::Noise);
        osc->setFrequency(4000.0f, kRate);
    }
    a.reset(7);
    b.reset(7);
    c.reset(8);
    const std::vector<float> na = run(a, 4800), nb = run(b, 4800), nc = run(c, 4800);
    CHECK(na == nb);
    CHECK(na != nc);
    double mean = 0.0;
    for (float v : na) mean += v;
    CHECK(std::abs(mean / double(na.size())) < 0.05);
    CHECK(rms(na) > 0.4f);

    Oscillator loop;
    loop.setWaveform(Waveform::NoiseLoop);
    loop.setFrequency(float(kRate), kRate); // One new value per sample
    const std::vector<float> bits = run(loop, 2000);
    // Short NES LFSR: period 93 (or 31) → sequence repeats
    bool periodic = true;
    for (std::size_t i = 0; i + 93 < bits.size(); i++) periodic = periodic && bits[i] == bits[i + 93];
    CHECK(periodic);
}

TEST_CASE("midiToHz: A440 and octaves") {
    CHECK(midiToHz(69.0f) == doctest::Approx(440.0f));
    CHECK(midiToHz(81.0f) == doctest::Approx(880.0f));
    CHECK(midiToHz(60.0f) == doctest::Approx(261.63f).epsilon(0.001));
}

TEST_CASE("ADSR envelope: attack times, decay to sustain, release to zero") {
    Envelope env;
    env.start({0.01f, 0.02f, 0.5f, 0.05f}, kRate);
    std::vector<float> levels;
    for (int i = 0; i < 4800; i++) levels.push_back(env.next()); // 0.1 s
    CHECK(levels[240] == doctest::Approx(0.5f).epsilon(0.02));   // Halfway through attack
    CHECK(*std::max_element(levels.begin(), levels.end()) == doctest::Approx(1.0f));
    CHECK(levels[480 + 480] == doctest::Approx(0.75f).epsilon(0.02)); // Halfway through decay
    CHECK(levels.back() == doctest::Approx(0.5f));                    // Sustain
    CHECK(env.stage() == Envelope::Stage::Sustain);
    env.noteOff();
    int samples = 0;
    while (!env.finished() && samples < 10000) {
        env.next();
        samples++;
    }
    CHECK(samples == doctest::Approx(2400).epsilon(0.02)); // 0.05 s
    CHECK(env.next() == 0.0f);

    // Percussive (sustain 0): finishes on its own
    env.start({0.0f, 0.05f, 0.0f, 0.1f}, kRate);
    samples = 0;
    while (!env.finished() && samples < 10000) {
        env.next();
        samples++;
    }
    CHECK(samples < 2500);
}

TEST_CASE("Filter: low-pass attenuates highs, high-pass attenuates lows, and stays stable at high resonance") {
    const auto filtered = [](FilterMode mode, float cutoff, float resonance, float hz) {
        Filter f;
        f.set(mode, cutoff, resonance, kRate);
        std::vector<float> in = sine(hz, kRate / 4), out(in.size());
        for (std::size_t i = 0; i < in.size(); i++) out[i] = f.process(in[i]);
        return rms(out, out.size() / 2) / rms(in, in.size() / 2);
    };
    CHECK(filtered(FilterMode::LowPass, 500.0f, 0.0f, 100.0f) > 0.95f);
    CHECK(filtered(FilterMode::LowPass, 500.0f, 0.0f, 8000.0f) < 0.02f);
    CHECK(filtered(FilterMode::HighPass, 2000.0f, 0.0f, 100.0f) < 0.01f);
    CHECK(filtered(FilterMode::HighPass, 2000.0f, 0.0f, 10000.0f) > 0.9f);
    CHECK(filtered(FilterMode::BandPass, 1000.0f, 0.5f, 1000.0f) > 0.9f);
    CHECK(filtered(FilterMode::None, 100.0f, 0.0f, 5000.0f) == doctest::Approx(1.0f));
    // Maximum resonance at cutoff: peaks without blowing up
    const float resonant = filtered(FilterMode::LowPass, 1000.0f, 1.0f, 1000.0f);
    CHECK(resonant > 2.0f);
    CHECK(resonant < 30.0f);
    // Out-of-range cutoff: clamped and stays stable
    Filter f;
    f.set(FilterMode::LowPass, 1e9f, 1.0f, kRate);
    float worst = 0.0f;
    for (float v : sine(15000.0f, 4800)) worst = std::max(worst, std::abs(f.process(v)));
    CHECK(std::isfinite(worst));
}

TEST_CASE("Delay: an impulse returns every delay seconds, getting quieter each time") {
    Delay delay(1.0f, kRate);
    delay.setSettings({0.1f, 0.5f, 1.0f});
    std::vector<float> out;
    for (int i = 0; i < kRate / 2; i++) {
        float l = 0.0f, r = 0.0f;
        delay.process(i == 0 ? 1.0f : 0.0f, 0.0f, l, r);
        out.push_back(l);
    }
    CHECK(out[4800] == doctest::Approx(1.0f));
    CHECK(out[9600] == doctest::Approx(0.5f));
    CHECK(out[14400] == doctest::Approx(0.25f));
    CHECK(out[4799] == doctest::Approx(0.0f));
}

TEST_CASE("Mixer: a note plays with its envelope, releases automatically after duration, and frees the voice") {
    Mixer mixer(kRate);
    Instrument lead;
    lead.wave = Waveform::Triangle;
    lead.envelope = {0.01f, 0.05f, 0.6f, 0.1f};
    lead.gain = 0.5f;
    const VoiceHandle note = mixer.playNote(lead, 330.0f, 1.0f, 0.3f);
    CHECK(note);
    const std::vector<float> out = left(renderMixer(mixer, kRate / 2 + 4800)); // 0.6 s
    CHECK(frequency(std::vector<float>(out.begin() + 1000, out.begin() + 14000), kRate) ==
          doctest::Approx(330.0f).epsilon(0.02));
    // Sustain 0.6 × gain 0.5 × cos 45°... center normalized to gain: peak ≈ 0.3
    CHECK(peak(out, 6000, 14000) == doctest::Approx(0.3f).epsilon(0.05));
    CHECK(peak(out, 0.45 * kRate) < 1e-4f); // 0.3 s + 0.1 s release: already finished
    CHECK(mixer.stats().activeVoices == 0);
}

TEST_CASE("Mixer: sustained notes, releaseNote, stopNote, and bus volume") {
    Mixer mixer(kRate);
    Instrument pad;
    pad.wave = Waveform::Sine;
    pad.envelope = {0.001f, 0.001f, 1.0f, 0.05f};
    pad.gain = 0.4f;
    const VoiceHandle a = mixer.playNote(pad, 200.0f, 1.0f, 0.0f, AudioBus::Music);
    const VoiceHandle b = mixer.playNote(pad, 300.0f, 1.0f, 0.0f);
    renderMixer(mixer, kRate); // One second: still playing
    CHECK(mixer.stats().activeVoices == 2);

    mixer.setBusVolume(AudioBus::Music, 0.0f);
    mixer.stopNote(b);
    std::vector<float> out = left(renderMixer(mixer, 4800));
    CHECK(peak(out, 100) < 1e-4f); // b stopped, a muted by its bus
    CHECK(mixer.stats().activeVoices == 1);

    mixer.setBusVolume(AudioBus::Music, 1.0f);
    mixer.releaseNote(a);
    renderMixer(mixer, 4800); // 0.1 s > release
    CHECK(mixer.stats().activeVoices == 0);
    CHECK_FALSE(mixer.releaseNote(VoiceHandle{}));
}

TEST_CASE("Mixer: with no free voices, steals a releasing voice first") {
    Mixer mixer(kRate);
    Instrument held;
    held.envelope = {0.001f, 0.001f, 1.0f, 1.0f}; // Long release
    VoiceHandle first;
    for (std::size_t i = 0; i < Mixer::kMaxVoices; i++) {
        const VoiceHandle h = mixer.playNote(held, 100.0f + float(i), 1.0f, 0.0f);
        if (i == 5) first = h;
    }
    renderMixer(mixer, 480);
    mixer.releaseNote(first);
    renderMixer(mixer, 480);
    const VoiceHandle extra = mixer.playNote(held, 1000.0f, 1.0f, 0.0f);
    renderMixer(mixer, 480);
    CHECK(mixer.stats().stolenVoices == 1);
    CHECK(mixer.stats().activeVoices == Mixer::kMaxVoices);
    // The stolen voice was the releasing one: releasing it finds nothing, the new one keeps playing
    mixer.stopNote(extra);
    renderMixer(mixer, 480);
    CHECK(mixer.stats().activeVoices == Mixer::kMaxVoices - 1);
}

TEST_CASE("Mixer: delay send, vibrato, and pitch slide") {
    Mixer mixer(kRate);
    mixer.setDelay({0.2f, 0.0f, 1.0f});
    Instrument blip;
    blip.wave = Waveform::Sine;
    blip.envelope = {0.001f, 0.001f, 1.0f, 0.001f};
    blip.delaySend = 1.0f;
    mixer.playNote(blip, 1000.0f, 1.0f, 0.05f);
    const std::vector<float> out = left(renderMixer(mixer, kRate / 2));
    CHECK(peak(out, 0, 2400) > 0.3f);
    CHECK(peak(out, 4000, 9000) < 1e-3f); // Silence between the note and the echo
    CHECK(peak(out, 9700, 11800) > 0.3f); // Echo at 0.2 s

    Mixer slide(kRate);
    Instrument up = blip;
    up.delaySend = 0.0f;
    up.pitchSlide = 12.0f; // One octave per second
    slide.playNote(up, 200.0f, 1.0f, 1.0f);
    const std::vector<float> rising = left(renderMixer(slide, kRate));
    const float start = frequency(std::vector<float>(rising.begin(), rising.begin() + 4800), kRate);
    const float end = frequency(std::vector<float>(rising.end() - 4800, rising.end()), kRate);
    CHECK(start == doctest::Approx(206.0f).epsilon(0.05));
    CHECK(end == doctest::Approx(390.0f).epsilon(0.05));
}

TEST_CASE("softClip: linear in the middle, never exceeds 1") {
    CHECK(softClip(0.5f) == 0.5f);
    CHECK(softClip(-0.8f) == -0.8f);
    CHECK(softClip(1.0f) < 1.0f);
    CHECK(softClip(1.0f) > 0.9f);
    CHECK(softClip(100.0f) <= 1.0f);
    CHECK(softClip(-100.0f) >= -1.0f);
}
