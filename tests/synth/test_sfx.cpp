// Efectos de sonido paramétricos (#48): cada preset tiene la forma que promete su nombre
#include <doctest/doctest.h>
#include <mge/synth/Offline.h>
#include <mge/synth/Sfx.h>

#include <cmath>

using namespace mge;
namespace an = mge::analysis;

namespace {
    constexpr int kRate = 48000;
    std::size_t at(float seconds) {
        return std::size_t(seconds * kRate);
    }
    float pitch(const Sample& s, float from, float to) {
        return an::zeroCrossingHz(s.data, kRate, at(from), at(to));
    }
}

TEST_CASE("renderSfx: largo = attack + sustain + decay, determinista, en [-1, 1]") {
    for (const auto& preset :
         {sfx::jump(), sfx::coin(), sfx::hurt(), sfx::explosion(), sfx::powerUp(), sfx::click()}) {
        const Sample a = renderSfx(preset, kRate);
        CHECK(a.channels == 1);
        CHECK(double(a.frameCount()) / kRate == doctest::Approx(preset.duration()).epsilon(0.01));
        CHECK(a.data == renderSfx(preset, kRate).data);
        CHECK(an::peak(a.data) <= 1.0f);
        CHECK(an::peak(a.data) > 0.1f);
        // Termina en silencio (sin chasquido al cortar)
        CHECK(std::abs(a.data.back()) < 0.02f);
    }
}

TEST_CASE("Presets: el salto sube, el golpe baja, la moneda son dos notas") {
    const Sample jump = renderSfx(sfx::jump(), kRate);
    CHECK(pitch(jump, 0.08f, 0.15f) > 1.3f * pitch(jump, 0.0f, 0.05f));

    const Sample hurt = renderSfx(sfx::hurt(), kRate);
    CHECK(pitch(hurt, 0.1f, 0.18f) < 0.6f * pitch(hurt, 0.0f, 0.04f));

    const Sample coin = renderSfx(sfx::coin(), kRate);
    CHECK(pitch(coin, 0.005f, 0.065f) == doctest::Approx(987.77f).epsilon(0.03));
    CHECK(pitch(coin, 0.08f, 0.2f) == doctest::Approx(987.77f * 1.335f).epsilon(0.03));
}

TEST_CASE(
    "Presets: la explosión es ruido que se oscurece; el power-up sube varias veces; el click es corto") {
    const Sample boom = renderSfx(sfx::explosion(), kRate);
    const float early = an::spectralCentroid(boom.data, kRate, at(0.02f));
    const float late = an::spectralCentroid(boom.data, kRate, at(0.6f));
    CHECK(early > 800.0f);
    CHECK(late < 0.6f * early);
    CHECK(an::toneAmplitude(boom.data, 140.0f, kRate, 0, at(0.2f)) < 0.2f); // No es un tono

    const Sample power = renderSfx(sfx::powerUp(), kRate);
    // Cada 0.11 s vuelve a empezar más grave y sube
    CHECK(pitch(power, 0.07f, 0.105f) > 1.15f * pitch(power, 0.005f, 0.04f));
    CHECK(pitch(power, 0.115f, 0.15f) < pitch(power, 0.07f, 0.105f));
    CHECK(pitch(power, 0.18f, 0.215f) > 1.15f * pitch(power, 0.115f, 0.15f));

    const Sample click = renderSfx(sfx::click(), kRate);
    CHECK(double(click.frameCount()) / kRate < 0.06);
}

TEST_CASE("Variaciones: con semilla cambian un poco, randomize siempre da algo audible") {
    const SfxParams base = sfx::coin();
    const SfxParams a = sfx::coin(1), b = sfx::coin(2);
    CHECK(a.baseFrequency != base.baseFrequency);
    CHECK(a.baseFrequency != b.baseFrequency);
    CHECK(a.baseFrequency == doctest::Approx(base.baseFrequency).epsilon(0.08)); // Leve
    CHECK(sfx::coin(1).baseFrequency == a.baseFrequency);                        // Determinista

    const SfxParams wild = sfx::mutate(base, 1.0f, 3);
    CHECK(wild.duty >= 0.05f);
    CHECK(wild.duty <= 0.95f);
    CHECK(wild.sustain > 0.0f);

    for (std::uint64_t seed = 1; seed <= 40; seed++) {
        CAPTURE(seed);
        const SfxParams p = sfx::randomize(seed);
        const Sample s = renderSfx(p, kRate, seed);
        CHECK(an::peak(s.data) <= 1.0f);
        CHECK(an::rms(s.data) > 0.005f);
        bool finite = true;
        for (float v : s.data) finite = finite && std::isfinite(v);
        CHECK(finite);
    }
    CHECK(sfx::randomize(5).baseFrequency == sfx::randomize(5).baseFrequency);
}
