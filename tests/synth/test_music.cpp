// Music sequencer (#49): sample-accurate notes, looping, crossfades, pattern parser
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

    // 120 BPM, 4 steps per beat → one step = 0.125 s = 6000 frames
    Song clickSong(const std::string& pattern) {
        Song song(120.0f, 4);
        const int i = song.addInstrument(blip());
        song.addTrack(i, {song.pattern(pattern)});
        return song;
    }
}

TEST_CASE("noteFromName and parsePattern") {
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
    CHECK(p->notes[2].note == 36.0f); // "x" = default note
    CHECK(p->notes[3].length == 3);
    CHECK(p->notes[3].velocity == doctest::Approx(0.8f));

    CHECK_FALSE(parsePattern("C4 ? E4", 1.0f, 60.0f, &error));
    CHECK(error.find("step 2") != std::string::npos);
    CHECK_FALSE(parsePattern("  |  ", 1.0f, 60.0f, &error));
}

TEST_CASE("Song: length, ordered events, errors") {
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
    CHECK(events[1].step == 0); // Both tracks
    CHECK(events.back().step == 16);
    for (std::size_t i = 1; i < events.size(); i++) CHECK(events[i].step >= events[i - 1].step);

    Song bad(120.0f);
    CHECK_FALSE(bad.valid()); // No tracks
    bad.addTrack(3, {0});
    CHECK(bad.error().find("instrument") != std::string::npos);
    Song parseError;
    parseError.pattern("C4 Z9");
    CHECK_FALSE(parseError.valid());

    Mixer mixer(kRate);
    CHECK(mixer.addSong(bad) == kInvalidSong);
    CHECK_FALSE(mixer.playSong(7));
}

TEST_CASE("Music: each note starts on its sample and the song repeats from the loop point") {
    Mixer mixer(kRate);
    Song song = clickSong("C5 . C5 . | C5 C5 . .");
    song.setLoopStart(4);
    const SongId id = mixer.addSong(song);
    REQUIRE(id != kInvalidSong);
    REQUIRE(mixer.playSong(id));
    // 8 steps (1 s) + return to step 4: another 4 steps (0.5 s) → 1.5 s, two loop iterations
    const std::vector<float> left = an::channel(renderOffline(mixer, 2.0f), 2, 0);
    const std::vector<std::size_t> onsets = an::onsets(left, kRate, 0.05f);
    // Steps 0, 2, 4, 5 | loop: 8→4: 8, 9 | 12, 13
    const std::vector<std::size_t> expected = {0, 2, 4, 5, 8, 9, 12, 13};
    REQUIRE(onsets.size() == expected.size());
    for (std::size_t i = 0; i < expected.size(); i++) {
        CAPTURE(i);
        // 5 ms windows (240 frames): onset falls in the step's window
        CHECK(onsets[i] / 240 == expected[i] * 6000 / 240);
    }
    CHECK(mixer.musicStep() >= 0);
    mixer.stopMusic();
    renderOffline(mixer, 0.05f);
    CHECK(mixer.musicStep() == -1);
    CHECK(an::peak(an::channel(renderOffline(mixer, 0.2f), 2, 0)) < 1e-4f);
}

TEST_CASE("Music: crossfade without volume jumps and Music bus") {
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
    // During the crossfade, A3 fades out and A5 fades in gradually
    const std::size_t w = kRate / 10;
    float previousLow = an::toneAmplitude(left, 220.0f, kRate, 0, w);
    float previousHigh = an::toneAmplitude(left, 880.0f, kRate, 0, w);
    CHECK(previousLow > 0.3f);
    for (std::size_t i = 1; i < 10; i++) {
        const float lowLevel = an::toneAmplitude(left, 220.0f, kRate, i * w, (i + 1) * w);
        const float highLevel = an::toneAmplitude(left, 880.0f, kRate, i * w, (i + 1) * w);
        CHECK(lowLevel <= previousLow + 0.01f);
        CHECK(highLevel >= previousHigh - 0.01f);
        CHECK(lowLevel > previousLow - 0.15f); // No jumps
        previousLow = lowLevel;
        previousHigh = highLevel;
    }
    CHECK(an::toneAmplitude(left, 220.0f, kRate, kRate * 11 / 10, kRate * 3 / 2) < 0.01f);
    CHECK(an::toneAmplitude(left, 880.0f, kRate, kRate * 11 / 10, kRate * 3 / 2) > 0.3f);

    mixer.setBusVolume(AudioBus::Music, 0.0f);
    const std::vector<float> muted = an::channel(renderOffline(mixer, 0.2f), 2, 0);
    CHECK(an::peak(muted) < 1e-4f);
}

TEST_CASE("Built-in songs: valid, audible, and do not clip") {
    for (const Song& song : {songs::platformerTheme(), songs::sandboxAmbient(), songs::sandboxNight()}) {
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
