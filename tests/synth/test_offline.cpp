// Audio sin dispositivo (#50): render offline, WAV y valores de referencia del sintetizador
#include <doctest/doctest.h>
#include <mge/synth/Offline.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>

using namespace mge;
namespace an = mge::analysis;

namespace {
    constexpr int kRate = 48000;

    std::vector<float> renderNote(const Instrument& instrument, float hz, float seconds, float duration) {
        Mixer mixer(kRate);
        mixer.playNote(instrument, hz, 1.0f, duration);
        return an::channel(renderOffline(mixer, seconds), 2, 0);
    }

    Instrument plain(Waveform wave) {
        Instrument in;
        in.wave = wave;
        in.envelope = {0.002f, 0.001f, 1.0f, 0.01f};
        in.gain = 0.5f;
        return in;
    }
}

TEST_CASE("Offline: renderOffline da frames × 2 y es determinista") {
    const auto once = [] {
        Mixer mixer(kRate);
        Instrument noisy = plain(Waveform::Noise);
        noisy.filter = FilterMode::BandPass;
        noisy.cutoff = 1500.0f;
        mixer.playNote(noisy, 800.0f, 1.0f, 0.2f);
        mixer.playNote(plain(Waveform::Saw), 220.0f, 0.7f, 0.3f);
        mixer.setDelay({0.1f, 0.3f, 0.4f});
        return renderOffline(mixer, 0.5f);
    };
    const std::vector<float> a = once();
    CHECK(a.size() == std::size_t(kRate / 2) * 2);
    CHECK(a == once()); // Mismo resultado byte a byte
}

TEST_CASE("Offline: writeWav escribe un PCM de 16 bits que se puede leer") {
    const std::filesystem::path path = std::filesystem::temp_directory_path() / "mge_test_offline.wav";
    std::vector<float> stereo = {0.0f, 0.0f, 0.5f, -0.5f, 1.0f, -1.0f, 2.0f, -2.0f};
    REQUIRE(writeWav(path, stereo, 2, kRate));
    std::ifstream in(path, std::ios::binary);
    const std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(in)),
                                           std::istreambuf_iterator<char>());
    REQUIRE(bytes.size() == 44 + stereo.size() * 2);
    CHECK(std::string(bytes.begin(), bytes.begin() + 4) == "RIFF");
    CHECK(std::string(bytes.begin() + 8, bytes.begin() + 16) == "WAVEfmt ");
    const auto u16 = [&](std::size_t at) { return std::uint16_t(bytes[at] | (bytes[at + 1] << 8)); };
    const auto u32 = [&](std::size_t at) {
        return std::uint32_t(u16(at) | (std::uint32_t(u16(at + 2)) << 16));
    };
    CHECK(u16(22) == 2);                    // Canales
    CHECK(u32(24) == 48000u);               // Frecuencia
    CHECK(u16(34) == 16);                   // Bits
    CHECK(u32(40) == 16u);                  // Bytes de datos
    CHECK(std::int16_t(u16(48)) == 16384);  // 0.5
    CHECK(std::int16_t(u16(50)) == -16384); // -0.5
    CHECK(std::int16_t(u16(56)) == 32767);  // 2.0 se recorta
    CHECK(std::int16_t(u16(58)) == -32767);
    std::filesystem::remove(path);
    CHECK_FALSE(writeWav("/no/existe/x.wav", stereo, 2, kRate));
}

TEST_CASE("Valores de referencia: cada forma de onda (frecuencia, nivel y brillo)") {
    // Centroide espectral: seno < triangular < cuadrada < sierra < ruido
    float previous = 0.0f;
    for (Waveform wave :
         {Waveform::Sine, Waveform::Triangle, Waveform::Square, Waveform::Saw, Waveform::Noise}) {
        CAPTURE(int(wave));
        const std::vector<float> note = renderNote(plain(wave), 440.0f, 0.3f, 0.25f);
        if (wave != Waveform::Noise) {
            CHECK(an::zeroCrossingHz(note, kRate, 2000, 10000) == doctest::Approx(440.0f).epsilon(0.02));
            CHECK(an::toneAmplitude(note, 440.0f, kRate, 2000, 10000) > 0.1f);
        }
        const float centroid = an::spectralCentroid(note, kRate, 2000);
        CHECK(centroid > previous);
        previous = centroid;
        CHECK(an::peak(note) <= 1.0f);
    }
    // Seno: amplitud exacta en 440 Hz y nada en 880
    const std::vector<float> sine = renderNote(plain(Waveform::Sine), 440.0f, 0.3f, 0.25f);
    CHECK(an::toneAmplitude(sine, 440.0f, kRate, 2000, 10000) == doctest::Approx(0.5f).epsilon(0.03));
    CHECK(an::toneAmplitude(sine, 880.0f, kRate, 2000, 10000) < 0.01f);
    // Cuadrada: armónicos impares (3º = 1/3 del fundamental), sin pares
    const std::vector<float> square = renderNote(plain(Waveform::Square), 440.0f, 0.3f, 0.25f);
    const float fundamental = an::toneAmplitude(square, 440.0f, kRate, 2000, 10000);
    CHECK(an::toneAmplitude(square, 1320.0f, kRate, 2000, 10000) / fundamental ==
          doctest::Approx(1.0f / 3.0f).epsilon(0.1));
    CHECK(an::toneAmplitude(square, 880.0f, kRate, 2000, 10000) / fundamental < 0.05f);
}

TEST_CASE("Valores de referencia: envolvente y filtro en el tiempo") {
    Instrument pluck = plain(Waveform::Saw);
    pluck.envelope = {0.01f, 0.1f, 0.3f, 0.2f};
    const std::vector<float> note = renderNote(pluck, 220.0f, 1.0f, 0.4f);
    const std::size_t ms = kRate / 1000;
    CHECK(an::rms(note, 0, 5 * ms) < an::rms(note, 10 * ms, 30 * ms)); // Ataque
    CHECK(an::rms(note, 200 * ms, 380 * ms) ==
          doctest::Approx(0.3f * an::rms(note, 10 * ms, 15 * ms)).epsilon(0.15));
    // Termina cerca de duration + release
    CHECK(double(an::lastAbove(note, 1e-3f)) / kRate == doctest::Approx(0.6).epsilon(0.05));

    // Pasa bajos: más oscuro cuanto más bajo el corte
    float previous = 1e9f;
    for (float cutoff : {6000.0f, 2000.0f, 600.0f}) {
        Instrument dark = plain(Waveform::Saw);
        dark.filter = FilterMode::LowPass;
        dark.cutoff = cutoff;
        const float centroid = an::spectralCentroid(renderNote(dark, 110.0f, 0.3f, 0.25f), kRate, 2000);
        CHECK(centroid < previous);
        previous = centroid;
    }
}

TEST_CASE("onsets encuentra el comienzo de cada nota") {
    Mixer mixer(kRate);
    const Instrument blip = [] {
        Instrument in = plain(Waveform::Square);
        in.envelope = {0.001f, 0.001f, 1.0f, 0.005f};
        return in;
    }();
    std::vector<float> out;
    for (int i = 0; i < 4; i++) {
        mixer.playNote(blip, 600.0f, 1.0f, 0.05f);
        const std::vector<float> block = renderOffline(mixer, 0.25f);
        out.insert(out.end(), block.begin(), block.end());
    }
    const std::vector<std::size_t> found = an::onsets(an::channel(out, 2, 0), kRate, 0.05f);
    REQUIRE(found.size() == 4);
    for (std::size_t i = 0; i < 4; i++)
        CHECK(double(found[i]) / kRate == doctest::Approx(0.25 * double(i)).epsilon(0.01));
}
