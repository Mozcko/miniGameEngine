// Síntesis (#47): osciladores, envolventes, filtros, eco y notas del mezclador
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

TEST_CASE("Oscilador: cada forma de onda tiene la frecuencia pedida y queda en [-1, 1]") {
    for (Waveform wave : {Waveform::Sine, Waveform::Square, Waveform::Saw, Waveform::Triangle}) {
        CAPTURE(int(wave));
        Oscillator osc;
        osc.setWaveform(wave);
        osc.setFrequency(220.0f, kRate);
        const std::vector<float> out = run(osc, kRate);
        CHECK(frequency(out, kRate) == doctest::Approx(220.0f).epsilon(0.01));
        CHECK(peak(out) <= 1.1f); // PolyBLEP puede pasarse un poco en los saltos
        CHECK(peak(out) >= 0.95f);
    }
    // Valores RMS conocidos: seno 1/√2, cuadrada 1, triangular 1/√3, sierra 1/√3
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

TEST_CASE("Oscilador: el ancho de pulso cambia cuánto tiempo la cuadrada está arriba") {
    Oscillator osc;
    osc.setWaveform(Waveform::Square);
    osc.setPulseWidth(0.25f);
    osc.setFrequency(100.0f, kRate);
    const std::vector<float> out = run(osc, kRate);
    std::size_t high = 0;
    for (float v : out) high += v > 0.0f ? 1 : 0;
    CHECK(float(high) / float(out.size()) == doctest::Approx(0.25f).epsilon(0.02));
}

TEST_CASE("Oscilador: ruido determinista por semilla, centrado; el de 8 bits se repite") {
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
    loop.setFrequency(float(kRate), kRate); // Un valor nuevo por muestra
    const std::vector<float> bits = run(loop, 2000);
    // LFSR corto de la NES: período 93 (o 31) → la secuencia se repite
    bool periodic = true;
    for (std::size_t i = 0; i + 93 < bits.size(); i++) periodic = periodic && bits[i] == bits[i + 93];
    CHECK(periodic);
}

TEST_CASE("midiToHz: La 440 y octavas") {
    CHECK(midiToHz(69.0f) == doctest::Approx(440.0f));
    CHECK(midiToHz(81.0f) == doctest::Approx(880.0f));
    CHECK(midiToHz(60.0f) == doctest::Approx(261.63f).epsilon(0.001));
}

TEST_CASE("Envolvente ADSR: tiempos de ataque, caída a sustain, release a cero") {
    Envelope env;
    env.start({0.01f, 0.02f, 0.5f, 0.05f}, kRate);
    std::vector<float> levels;
    for (int i = 0; i < 4800; i++) levels.push_back(env.next()); // 0.1 s
    CHECK(levels[240] == doctest::Approx(0.5f).epsilon(0.02));   // Mitad del ataque
    CHECK(*std::max_element(levels.begin(), levels.end()) == doctest::Approx(1.0f));
    CHECK(levels[480 + 480] == doctest::Approx(0.75f).epsilon(0.02)); // Mitad del decay
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

    // Percusiva (sustain 0): termina sola
    env.start({0.0f, 0.05f, 0.0f, 0.1f}, kRate);
    samples = 0;
    while (!env.finished() && samples < 10000) {
        env.next();
        samples++;
    }
    CHECK(samples < 2500);
}

TEST_CASE("Filtro: pasa bajos atenúa agudos, pasa altos atenúa graves, y es estable con resonancia alta") {
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
    // Resonancia máxima en el corte: hay pico pero no explota
    const float resonant = filtered(FilterMode::LowPass, 1000.0f, 1.0f, 1000.0f);
    CHECK(resonant > 2.0f);
    CHECK(resonant < 30.0f);
    // Corte fuera de rango: se limita, sigue estable
    Filter f;
    f.set(FilterMode::LowPass, 1e9f, 1.0f, kRate);
    float worst = 0.0f;
    for (float v : sine(15000.0f, 4800)) worst = std::max(worst, std::abs(f.process(v)));
    CHECK(std::isfinite(worst));
}

TEST_CASE("Eco: un impulso vuelve a los delay segundos, cada vez más bajo") {
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

TEST_CASE("Mixer: una nota suena con su envolvente, se suelta sola tras duration y libera la voz") {
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
    // Sustain 0.6 × ganancia 0.5 × cos 45°... centro normalizado a la ganancia: pico ≈ 0.3
    CHECK(peak(out, 6000, 14000) == doctest::Approx(0.3f).epsilon(0.05));
    CHECK(peak(out, 0.45 * kRate) < 1e-4f); // 0.3 s + 0.1 s de release: ya terminó
    CHECK(mixer.stats().activeVoices == 0);
}

TEST_CASE("Mixer: notas sostenidas, releaseNote, stopNote y volumen por grupo") {
    Mixer mixer(kRate);
    Instrument pad;
    pad.wave = Waveform::Sine;
    pad.envelope = {0.001f, 0.001f, 1.0f, 0.05f};
    pad.gain = 0.4f;
    const VoiceHandle a = mixer.playNote(pad, 200.0f, 1.0f, 0.0f, AudioBus::Music);
    const VoiceHandle b = mixer.playNote(pad, 300.0f, 1.0f, 0.0f);
    renderMixer(mixer, kRate); // Un segundo: siguen sonando
    CHECK(mixer.stats().activeVoices == 2);

    mixer.setBusVolume(AudioBus::Music, 0.0f);
    mixer.stopNote(b);
    std::vector<float> out = left(renderMixer(mixer, 4800));
    CHECK(peak(out, 100) < 1e-4f); // b cortada, a muda por su grupo
    CHECK(mixer.stats().activeVoices == 1);

    mixer.setBusVolume(AudioBus::Music, 1.0f);
    mixer.releaseNote(a);
    renderMixer(mixer, 4800); // 0.1 s > release
    CHECK(mixer.stats().activeVoices == 0);
    CHECK_FALSE(mixer.releaseNote(VoiceHandle{}));
}

TEST_CASE("Mixer: sin voces libres roba primero una que se está apagando") {
    Mixer mixer(kRate);
    Instrument held;
    held.envelope = {0.001f, 0.001f, 1.0f, 1.0f}; // Release largo
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
    // La robada fue la que estaba en release: soltarla ya no encuentra nada, la nueva sigue
    mixer.stopNote(extra);
    renderMixer(mixer, 480);
    CHECK(mixer.stats().activeVoices == Mixer::kMaxVoices - 1);
}

TEST_CASE("Mixer: envío al eco, vibrato y deslizamiento de tono") {
    Mixer mixer(kRate);
    mixer.setDelay({0.2f, 0.0f, 1.0f});
    Instrument blip;
    blip.wave = Waveform::Sine;
    blip.envelope = {0.001f, 0.001f, 1.0f, 0.001f};
    blip.delaySend = 1.0f;
    mixer.playNote(blip, 1000.0f, 1.0f, 0.05f);
    const std::vector<float> out = left(renderMixer(mixer, kRate / 2));
    CHECK(peak(out, 0, 2400) > 0.3f);
    CHECK(peak(out, 4000, 9000) < 1e-3f); // Silencio entre la nota y el eco
    CHECK(peak(out, 9700, 11800) > 0.3f); // El eco a los 0.2 s

    Mixer slide(kRate);
    Instrument up = blip;
    up.delaySend = 0.0f;
    up.pitchSlide = 12.0f; // Una octava por segundo
    slide.playNote(up, 200.0f, 1.0f, 1.0f);
    const std::vector<float> rising = left(renderMixer(slide, kRate));
    const float start = frequency(std::vector<float>(rising.begin(), rising.begin() + 4800), kRate);
    const float end = frequency(std::vector<float>(rising.end() - 4800, rising.end()), kRate);
    CHECK(start == doctest::Approx(206.0f).epsilon(0.05));
    CHECK(end == doctest::Approx(390.0f).epsilon(0.05));
}

TEST_CASE("softClip: lineal en el medio, nunca pasa de 1") {
    CHECK(softClip(0.5f) == 0.5f);
    CHECK(softClip(-0.8f) == -0.8f);
    CHECK(softClip(1.0f) < 1.0f);
    CHECK(softClip(1.0f) > 0.9f);
    CHECK(softClip(100.0f) <= 1.0f);
    CHECK(softClip(-100.0f) >= -1.0f);
}
