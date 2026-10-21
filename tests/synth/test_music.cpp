// Secuenciador de música (#49): notas en el tiempo exacto, loop, fundidos, parser de patrones
#include <doctest/doctest.h>
#include <mge/synth/Mixer.h>
#include <mge/synth/Music.h>
#include <mge/synth/Offline.h>

#include <string>

using namespace mge;
namespace an = mge::analysis;

namespace {
    constexpr int kRate = 48000;

    Instrument blip() {
        Instrument in;
        in.wave = Waveform::Square;
        in.envelope = {0.0005f, 0.001f, 1.0f, 0.002f};
        in.gain = 0.5f;
        return in;
    }

    // 120 BPM, 4 pasos por pulso → un paso = 0.125 s = 6000 frames
    Song clickSong(const std::string& pattern) {
        Song song(120.0f, 4);
        const int i = song.addInstrument(blip());
        song.addTrack(i, {song.pattern(pattern)});
        return song;
    }
}

TEST_CASE("noteFromName y parsePattern") {
    CHECK(noteFromName("C4") == 60.0f);
    CHECK(noteFromName("A4") == 69.0f);
    CHECK(noteFromName("C#3") == 49.0f);
    CHECK(noteFromName("Db3") == 49.0f);
    CHECK(noteFromName("B-1") == 11.0f);
    CHECK_FALSE(noteFromName("H2"));
    CHECK_FALSE(noteFromName("C"));
    CHECK_FALSE(noteFromName("C4x"));

    std::string error;
    const auto p = parsePattern("C4 - . E4 | x . G4 - -", 0.8f, 36.0f, &error);
    REQUIRE_MESSAGE(p.has_value(), error);
    CHECK(p->steps == 9);
    REQUIRE(p->notes.size() == 4);
    CHECK(p->notes[0].note == 60.0f);
    CHECK(p->notes[0].length == 2);
    CHECK(p->notes[1].step == 3);
    CHECK(p->notes[2].note == 36.0f); // "x" = nota por defecto
    CHECK(p->notes[3].length == 3);
    CHECK(p->notes[3].velocity == doctest::Approx(0.8f));

    CHECK_FALSE(parsePattern("C4 ? E4", 1.0f, 60.0f, &error));
    CHECK(error.find("paso 2") != std::string::npos);
    CHECK_FALSE(parsePattern("  |  ", 1.0f, 60.0f, &error));
}

TEST_CASE("Song: largo, eventos en orden, errores") {
    Song song(120.0f, 4);
    const int lead = song.addInstrument(blip());
    const int a = song.pattern("C4 . . . E4 . . .");
    const int b = song.pattern("G4 . . .");
    song.addTrack(lead, {a, b, a});
    song.addTrack(lead, {b});
    CHECK(song.valid());
    CHECK(song.lengthSteps() == 20);
    CHECK(song.secondsPerStep() == doctest::Approx(0.125f));
    const auto events = song.events();
    REQUIRE(events.size() == 6);
    CHECK(events[0].step == 0);
    CHECK(events[1].step == 0); // Las dos pistas
    CHECK(events.back().step == 16);
    for (std::size_t i = 1; i < events.size(); i++) CHECK(events[i].step >= events[i - 1].step);

    Song bad(120.0f);
    CHECK_FALSE(bad.valid()); // Sin pistas
    bad.addTrack(3, {0});
    CHECK(bad.error().find("instrumento") != std::string::npos);
    Song parseError;
    parseError.pattern("C4 Z9");
    CHECK_FALSE(parseError.valid());

    Mixer mixer(kRate);
    CHECK(mixer.addSong(bad) == kInvalidSong);
    CHECK_FALSE(mixer.playSong(7));
}

TEST_CASE("Música: cada nota empieza en su muestra y la canción se repite desde el loop") {
    Mixer mixer(kRate);
    Song song = clickSong("C5 . C5 . | C5 C5 . .");
    song.setLoopStart(4);
    const SongId id = mixer.addSong(song);
    REQUIRE(id != kInvalidSong);
    REQUIRE(mixer.playSong(id));
    // 8 pasos (1 s) + vuelta al paso 4: otros 4 pasos (0.5 s) → 1.5 s, dos veces el loop
    const std::vector<float> left = an::channel(renderOffline(mixer, 2.0f), 2, 0);
    const std::vector<std::size_t> onsets = an::onsets(left, kRate, 0.05f);
    // Pasos 0, 2, 4, 5 | loop: 8→4: 8, 9 | 12, 13
    const std::vector<std::size_t> expected = {0, 2, 4, 5, 8, 9, 12, 13};
    REQUIRE(onsets.size() == expected.size());
    for (std::size_t i = 0; i < expected.size(); i++) {
        CAPTURE(i);
        // Ventanas de 5 ms (240 frames): el comienzo cae en la ventana del paso
        CHECK(onsets[i] / 240 == expected[i] * 6000 / 240);
    }
    CHECK(mixer.musicStep() >= 0);
    mixer.stopMusic();
    renderOffline(mixer, 0.05f);
    CHECK(mixer.musicStep() == -1);
    CHECK(an::peak(an::channel(renderOffline(mixer, 0.2f), 2, 0)) < 1e-4f);
}

TEST_CASE("Música: fundido cruzado sin saltos de volumen y grupo Music") {
    Mixer mixer(kRate);
    Instrument held;
    held.wave = Waveform::Sine;
    held.envelope = {0.001f, 0.001f, 1.0f, 0.01f};
    held.gain = 0.5f;
    Song low(60.0f, 1), high(60.0f, 1);
    low.addTrack(low.addInstrument(held), {low.pattern("A3 - - - - - - -")});
    high.addTrack(high.addInstrument(held), {high.pattern("A5 - - - - - - -")});
    const SongId a = mixer.addSong(low), b = mixer.addSong(high);
    mixer.playSong(a);
    renderOffline(mixer, 0.5f);
    mixer.playSong(b, 1.0f);
    const std::vector<float> left = an::channel(renderOffline(mixer, 1.5f), 2, 0);
    // Durante el fundido el nivel de A3 baja y el de A5 sube, de a poco
    const std::size_t w = kRate / 10;
    float previousLow = an::toneAmplitude(left, 220.0f, kRate, 0, w);
    float previousHigh = an::toneAmplitude(left, 880.0f, kRate, 0, w);
    CHECK(previousLow > 0.3f);
    for (std::size_t i = 1; i < 10; i++) {
        const float lowLevel = an::toneAmplitude(left, 220.0f, kRate, i * w, (i + 1) * w);
        const float highLevel = an::toneAmplitude(left, 880.0f, kRate, i * w, (i + 1) * w);
        CHECK(lowLevel <= previousLow + 0.01f);
        CHECK(highLevel >= previousHigh - 0.01f);
        CHECK(lowLevel > previousLow - 0.15f); // Sin saltos
        previousLow = lowLevel;
        previousHigh = highLevel;
    }
    CHECK(an::toneAmplitude(left, 220.0f, kRate, kRate * 11 / 10, kRate * 3 / 2) < 0.01f);
    CHECK(an::toneAmplitude(left, 880.0f, kRate, kRate * 11 / 10, kRate * 3 / 2) > 0.3f);

    mixer.setBusVolume(AudioBus::Music, 0.0f);
    const std::vector<float> muted = an::channel(renderOffline(mixer, 0.2f), 2, 0);
    CHECK(an::peak(muted) < 1e-4f);
}

TEST_CASE("Canciones de ejemplo: válidas, suenan y no saturan") {
    for (const Song& song : {songs::platformerTheme(), songs::sandboxAmbient()}) {
        CAPTURE(song.bpm());
        REQUIRE_MESSAGE(song.valid(), song.error());
        Mixer mixer(kRate);
        mixer.setDelay({0.3f, 0.35f, 0.4f});
        mixer.playSong(mixer.addSong(song));
        const float seconds = std::min(6.0f, float(song.lengthSteps()) * song.secondsPerStep());
        const std::vector<float> left = an::channel(renderOffline(mixer, seconds), 2, 0);
        CHECK(an::rms(left) > 0.01f);
        CHECK(an::peak(left) < 0.99f);
    }
    CHECK(songs::platformerTheme().lengthSteps() == 64);
}
