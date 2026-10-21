#include <mge/synth/Sfx.h>

#include <mge/core/Random.h>
#include <mge/synth/Filter.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace mge {

    namespace {
        constexpr int kFilterUpdate = 32; // Recalcular los filtros cada tantas muestras (barrido)
    }

    Sample renderSfx(const SfxParams& p, int sampleRate, std::uint64_t seed) {
        const float rate = float(sampleRate);
        const std::size_t total = std::size_t(std::max(p.duration(), 0.001f) * rate);
        Sample out;
        out.channels = 1;
        out.data.assign(total, 0.0f);

        Oscillator osc;
        osc.setWaveform(p.wave);
        osc.reset(seed);
        Filter lowPass, highPass;
        std::vector<float> phaser(std::size_t(rate * 0.05f) + 2, 0.0f); // Hasta 50 ms
        std::size_t phaserWrite = 0;

        float slide = p.slide;
        float frequency = p.baseFrequency;
        float sinceRepeat = 0.0f;
        bool arpeggiated = false;
        const float dt = 1.0f / rate;
        for (std::size_t i = 0; i < total; i++) {
            const float t = float(i) * dt;
            // Repetición: vuelve al tono de partida
            if (p.repeatTime > 0.0f && sinceRepeat >= p.repeatTime) {
                sinceRepeat = 0.0f;
                frequency = p.baseFrequency;
                slide = p.slide;
                arpeggiated = false;
            }
            if (!arpeggiated && p.arpeggioTime > 0.0f && sinceRepeat >= p.arpeggioTime) {
                frequency *= p.arpeggioMultiplier;
                arpeggiated = true;
            }
            sinceRepeat += dt;
            slide += p.deltaSlide * dt;
            frequency *= std::exp2(slide * dt);
            if (frequency < p.minFrequency) break; // Bajó del mínimo: termina (el resto queda en silencio)
            float hz = frequency;
            if (p.vibratoDepth != 0.0f)
                hz *= std::exp2(p.vibratoDepth * std::sin(6.2831853f * p.vibratoSpeed * t) / 12.0f);
            osc.setFrequency(std::min(hz, rate * 0.49f), sampleRate);
            osc.setPulseWidth(p.duty + p.dutySweep * t);

            if (i % kFilterUpdate == 0) {
                if (p.lowPassCutoff > 0.0f)
                    lowPass.set(FilterMode::LowPass, p.lowPassCutoff * std::exp2(p.lowPassSweep * t),
                                p.lowPassResonance, sampleRate);
                if (p.highPassCutoff > 0.0f)
                    highPass.set(FilterMode::HighPass, p.highPassCutoff * std::exp2(p.highPassSweep * t),
                                 0.0f, sampleRate);
            }
            float value = highPass.process(lowPass.process(osc.next()));

            if (p.phaserOffset > 0.0f || p.phaserSweep != 0.0f) {
                const float ms = std::clamp(p.phaserOffset + p.phaserSweep * t, 0.0f, 49.0f);
                const std::size_t delay = std::size_t(ms * 0.001f * rate);
                const std::size_t read = (phaserWrite + phaser.size() - delay) % phaser.size();
                phaser[phaserWrite] = value;
                phaserWrite = (phaserWrite + 1) % phaser.size();
                value = 0.5f * (value + phaser[read]);
            }

            // Envolvente
            float envelope = 0.0f;
            if (t < p.attack) {
                envelope = t / p.attack;
            } else if (t < p.attack + p.sustain) {
                const float s = (t - p.attack) / std::max(p.sustain, 1e-6f);
                envelope = 1.0f + p.punch * 2.0f * (1.0f - s);
            } else {
                envelope = 1.0f - (t - p.attack - p.sustain) / std::max(p.decay, 1e-6f);
            }
            out.data[i] = std::clamp(value * std::max(envelope, 0.0f) * p.gain, -1.0f, 1.0f);
        }
        return out;
    }

    namespace sfx {
        namespace {
            // Variación leve para seed != 0
            SfxParams vary(const SfxParams& p, std::uint64_t seed) {
                return seed == 0 ? p : mutate(p, 0.1f, seed);
            }
        }

        SfxParams jump(std::uint64_t seed) {
            SfxParams p;
            p.wave = Waveform::Square;
            p.duty = 0.35f;
            p.baseFrequency = 330.0f;
            p.slide = 7.0f; // Sube: casi dos octavas en el sonido
            p.sustain = 0.06f;
            p.decay = 0.16f;
            p.highPassCutoff = 120.0f;
            p.gain = 0.35f;
            return vary(p, seed);
        }

        SfxParams coin(std::uint64_t seed) {
            SfxParams p;
            p.wave = Waveform::Square;
            p.duty = 0.5f;
            p.baseFrequency = 987.77f;     // Si 5
            p.arpeggioMultiplier = 1.335f; // → Mi 6
            p.arpeggioTime = 0.07f;
            p.sustain = 0.08f;
            p.punch = 0.4f;
            p.decay = 0.3f;
            p.gain = 0.3f;
            return vary(p, seed);
        }

        SfxParams hurt(std::uint64_t seed) {
            SfxParams p;
            p.wave = Waveform::Saw;
            p.baseFrequency = 520.0f;
            p.slide = -7.0f; // Cae rápido
            p.sustain = 0.04f;
            p.punch = 0.3f;
            p.decay = 0.18f;
            p.lowPassCutoff = 3500.0f;
            p.gain = 0.4f;
            return vary(p, seed);
        }

        SfxParams explosion(std::uint64_t seed) {
            SfxParams p;
            p.wave = Waveform::Noise;
            p.baseFrequency = 140.0f; // Ruido "afinado" grave: retumba
            p.slide = -1.0f;
            p.sustain = 0.15f;
            p.punch = 0.6f;
            p.decay = 0.65f;
            p.lowPassCutoff = 4000.0f;
            p.lowPassSweep = -4.0f; // Se oscurece
            p.phaserOffset = 3.0f;
            p.phaserSweep = 8.0f;
            p.gain = 0.6f;
            return vary(p, seed);
        }

        SfxParams powerUp(std::uint64_t seed) {
            SfxParams p;
            p.wave = Waveform::Square;
            p.duty = 0.3f;
            p.baseFrequency = 330.0f;
            p.slide = 6.0f;
            p.repeatTime = 0.11f; // Tres o cuatro subidas
            p.vibratoDepth = 0.4f;
            p.vibratoSpeed = 14.0f;
            p.sustain = 0.32f;
            p.decay = 0.15f;
            p.gain = 0.3f;
            return vary(p, seed);
        }

        SfxParams click(std::uint64_t seed) {
            SfxParams p;
            p.wave = Waveform::Square;
            p.baseFrequency = 1400.0f;
            p.slide = -3.0f;
            p.sustain = 0.008f;
            p.decay = 0.035f;
            p.highPassCutoff = 600.0f;
            p.gain = 0.3f;
            return vary(p, seed);
        }

        SfxParams randomize(std::uint64_t seed) {
            Pcg32 rng(seed, 0x5F5Fu);
            const auto range = [&](float lo, float hi) { return lo + (hi - lo) * rng.nextFloat01(); };
            SfxParams p;
            constexpr Waveform kWaves[] = {Waveform::Square, Waveform::Saw, Waveform::Triangle,
                                           Waveform::Sine, Waveform::Noise};
            p.wave = kWaves[rng.nextBounded(5)];
            p.duty = range(0.15f, 0.6f);
            p.baseFrequency = 110.0f * std::exp2(range(0.0f, 4.0f)); // 110 Hz .. 1.7 kHz
            p.slide = rng.nextFloat01() < 0.5f ? range(-4.0f, 4.0f) : 0.0f;
            p.vibratoDepth = rng.nextFloat01() < 0.3f ? range(0.1f, 1.0f) : 0.0f;
            p.vibratoSpeed = range(4.0f, 16.0f);
            if (rng.nextFloat01() < 0.3f) {
                p.arpeggioMultiplier = range(0.6f, 1.6f);
                p.arpeggioTime = range(0.03f, 0.15f);
            }
            p.attack = rng.nextFloat01() < 0.7f ? 0.0f : range(0.0f, 0.1f);
            p.sustain = range(0.02f, 0.25f);
            p.punch = range(0.0f, 0.6f);
            p.decay = range(0.05f, 0.4f);
            if (rng.nextFloat01() < 0.4f) {
                p.lowPassCutoff = range(800.0f, 8000.0f);
                p.lowPassSweep = range(-2.0f, 1.0f);
            }
            p.gain = 0.35f;
            return p;
        }

        SfxParams mutate(const SfxParams& params, float amount, std::uint64_t seed) {
            Pcg32 rng(seed, 0xA5A5u);
            const float a = std::clamp(amount, 0.0f, 1.0f);
            // Multiplicador cerca de 1 y desplazamiento cerca de 0
            const auto scale = [&](float v) { return v * std::exp2((rng.nextFloat01() * 2.0f - 1.0f) * a); };
            const auto shift = [&](float v, float span) {
                return v + (rng.nextFloat01() * 2.0f - 1.0f) * a * span;
            };
            SfxParams p = params;
            p.baseFrequency = std::clamp(scale(p.baseFrequency), 30.0f, 8000.0f);
            p.slide = shift(p.slide, 1.0f);
            p.vibratoDepth = std::max(0.0f, shift(p.vibratoDepth, 0.3f));
            p.duty = std::clamp(shift(p.duty, 0.2f), 0.05f, 0.95f);
            p.sustain = std::max(0.005f, scale(p.sustain));
            p.decay = std::max(0.01f, scale(p.decay));
            p.punch = std::clamp(shift(p.punch, 0.2f), 0.0f, 1.0f);
            if (p.lowPassCutoff > 0.0f)
                p.lowPassCutoff = std::clamp(scale(p.lowPassCutoff), 100.0f, 20000.0f);
            return p;
        }
    }
}
