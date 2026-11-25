#pragma once

#include <mge/synth/Instrument.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace mge {

    // A note inside a pattern (in steps: one step = 1/stepsPerBeat of a beat)
    struct NoteEvent {
        int step = 0;
        float note = 60.0f; // MIDI (60 = middle C)
        float velocity = 1.0f;
        int length = 1; // Steps
    };

    // A few bars of notes for one instrument
    struct Pattern {
        int steps = 16;
        std::vector<NoteEvent> notes;
    };

    // Parses a tracker-style pattern (#49). One step per whitespace-separated token:
    //   "C4"  "F#3" "Bb2"  note (MIDI: C4 = 60)       "."  rest
    //   "-"   holds the previous note one more step    "x"  default note (percussion)
    //   "|"   ignored (bar line, for readability only)
    // E.g.: "C3 . G2 . | C3 - - . " → 8 steps. Returns nullopt (and reason) on unrecognized input
    std::optional<Pattern> parsePattern(std::string_view text, float velocity = 1.0f,
                                        float defaultNote = 60.0f, std::string* error = nullptr);
    // "C4" → 60, "A4" → 69, "C#3"/"Db3" → 49. Returns nullopt if not a valid note
    std::optional<float> noteFromName(std::string_view name);

    // A song defined in C++ (#49): tempo, instruments, patterns, and tracks (each track plays a sequence of
    // patterns back to back on one instrument). The song lasts as long as its longest track and loops from
    // loopStart. Registered in the mixer (Mixer::addSong) and immutable thereafter.
    class Song {
    public:
        explicit Song(float bpm = 120.0f, int stepsPerBeat = 4) : m_bpm(bpm), m_stepsPerBeat(stepsPerBeat) {}

        int addInstrument(const Instrument& instrument);
        int addPattern(Pattern pattern);
        // Shorthand: parsePattern + addPattern. Returns -1 if text has errors (see error())
        int pattern(std::string_view text, float velocity = 1.0f, float defaultNote = 60.0f);
        // A track: instrument + sequence of patterns. Extra track gain
        void addTrack(int instrument, std::vector<int> patterns, float gain = 1.0f);
        void setLoopStart(int step) { m_loopStart = step; }

        float bpm() const { return m_bpm; }
        int stepsPerBeat() const { return m_stepsPerBeat; }
        float secondsPerStep() const { return 60.0f / (m_bpm * float(m_stepsPerBeat)); }
        int lengthSteps() const; // Longest track
        int loopStart() const { return m_loopStart; }
        // Empty if the song is valid (valid indices, positive tempo, something to play)
        std::string error() const;
        bool valid() const { return error().empty(); }

        // All notes sorted in time order, for the player
        struct Event {
            int step = 0;
            int instrument = 0;
            float frequency = 0.0f;
            float velocity = 1.0f;
            float seconds = 0.0f; // Hold duration before release
        };
        std::vector<Event> events() const;
        const std::vector<Instrument>& instruments() const { return m_instruments; }

    private:
        struct Track {
            int instrument = 0;
            std::vector<int> patterns;
            float gain = 1.0f;
        };
        float m_bpm;
        int m_stepsPerBeat;
        int m_loopStart = 0;
        std::vector<Instrument> m_instruments;
        std::vector<Pattern> m_patterns;
        std::vector<Track> m_tracks;
        std::string m_parseError;
    };

    // Example songs built with the sequencer (used by the platformer, sandbox, and gallery)
    namespace songs {
        Song platformerTheme(); // Upbeat chiptune: square lead, triangle bass, noise drums
        Song sandboxAmbient();  // Calm: soft filtered pads with echo, sparse bells
        Song sandboxNight();    // Nocturnal: slow calm pads with gentle chimes
    }
}
