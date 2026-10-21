#include <mge/synth/Music.h>

#include <algorithm>
#include <cctype>

namespace mge {

    std::optional<float> noteFromName(std::string_view name) {
        if (name.size() < 2) return std::nullopt;
        static constexpr int kSemitones[7] = {9, 11, 0, 2, 4, 5, 7}; // A B C D E F G
        const char letter = char(std::toupper(static_cast<unsigned char>(name[0])));
        if (letter < 'A' || letter > 'G') return std::nullopt;
        int semitone = kSemitones[letter - 'A'];
        std::size_t i = 1;
        if (name[i] == '#') {
            semitone++;
            i++;
        } else if (name[i] == 'b') {
            semitone--;
            i++;
        }
        bool negative = false;
        if (i < name.size() && name[i] == '-') {
            negative = true;
            i++;
        }
        if (i >= name.size()) return std::nullopt;
        int octave = 0;
        for (; i < name.size(); i++) {
            if (!std::isdigit(static_cast<unsigned char>(name[i]))) return std::nullopt;
            octave = octave * 10 + (name[i] - '0');
        }
        if (negative) octave = -octave;
        return float((octave + 1) * 12 + semitone);
    }

    std::optional<Pattern> parsePattern(std::string_view text, float velocity, float defaultNote,
                                        std::string* error) {
        Pattern pattern;
        pattern.steps = 0;
        std::size_t i = 0;
        while (i < text.size()) {
            while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i]))) i++;
            if (i >= text.size()) break;
            std::size_t end = i;
            while (end < text.size() && !std::isspace(static_cast<unsigned char>(text[end]))) end++;
            const std::string_view token = text.substr(i, end - i);
            i = end;
            if (token == "|") continue;
            if (token == ".") {
                pattern.steps++;
                continue;
            }
            if (token == "-") {
                if (!pattern.notes.empty() &&
                    pattern.notes.back().step + pattern.notes.back().length == pattern.steps)
                    pattern.notes.back().length++;
                pattern.steps++;
                continue;
            }
            std::optional<float> note =
                token == "x" ? std::optional<float>(defaultNote) : noteFromName(token);
            if (!note) {
                if (error)
                    *error = "paso " + std::to_string(pattern.steps + 1) + ": '" + std::string(token) +
                             "' no es una nota";
                return std::nullopt;
            }
            pattern.notes.push_back({pattern.steps, *note, velocity, 1});
            pattern.steps++;
        }
        if (pattern.steps == 0) {
            if (error) *error = "el patrón está vacío";
            return std::nullopt;
        }
        return pattern;
    }

    int Song::addInstrument(const Instrument& instrument) {
        m_instruments.push_back(instrument);
        return int(m_instruments.size()) - 1;
    }

    int Song::addPattern(Pattern pattern) {
        m_patterns.push_back(std::move(pattern));
        return int(m_patterns.size()) - 1;
    }

    int Song::pattern(std::string_view text, float velocity, float defaultNote) {
        std::string error;
        std::optional<Pattern> parsed = parsePattern(text, velocity, defaultNote, &error);
        if (!parsed) {
            if (m_parseError.empty()) m_parseError = error;
            return -1;
        }
        return addPattern(std::move(*parsed));
    }

    void Song::addTrack(int instrument, std::vector<int> patterns, float gain) {
        m_tracks.push_back({instrument, std::move(patterns), gain});
    }

    int Song::lengthSteps() const {
        int longest = 0;
        for (const Track& track : m_tracks) {
            int steps = 0;
            for (int p : track.patterns)
                if (p >= 0 && p < int(m_patterns.size())) steps += m_patterns[std::size_t(p)].steps;
            longest = std::max(longest, steps);
        }
        return longest;
    }

    std::string Song::error() const {
        if (!m_parseError.empty()) return m_parseError;
        if (!(m_bpm > 0.0f) || m_stepsPerBeat <= 0) return "tempo inválido";
        if (m_tracks.empty()) return "la canción no tiene pistas";
        for (const Track& track : m_tracks) {
            if (track.instrument < 0 || track.instrument >= int(m_instruments.size()))
                return "pista con un instrumento que no existe";
            for (int p : track.patterns)
                if (p < 0 || p >= int(m_patterns.size())) return "pista con un patrón que no existe";
        }
        if (lengthSteps() == 0) return "la canción no dura nada";
        if (m_loopStart < 0 || m_loopStart >= lengthSteps())
            return "el inicio del loop está fuera de la canción";
        return {};
    }

    std::vector<Song::Event> Song::events() const {
        std::vector<Event> events;
        if (!valid()) return events;
        const float stepSeconds = secondsPerStep();
        for (const Track& track : m_tracks) {
            int offset = 0;
            for (int p : track.patterns) {
                const Pattern& pattern = m_patterns[std::size_t(p)];
                for (const NoteEvent& n : pattern.notes) {
                    if (n.step < 0 || n.step >= pattern.steps) continue;
                    Event e;
                    e.step = offset + n.step;
                    e.instrument = track.instrument;
                    e.frequency = midiToHz(n.note);
                    e.velocity = std::clamp(n.velocity * track.gain, 0.0f, 1.0f);
                    e.seconds = float(std::max(n.length, 1)) * stepSeconds * 0.92f; // Un poco separado
                    events.push_back(e);
                }
                offset += pattern.steps;
            }
        }
        std::stable_sort(events.begin(), events.end(),
                         [](const Event& a, const Event& b) { return a.step < b.step; });
        return events;
    }

    namespace songs {
        Song platformerTheme() {
            Song song(132.0f, 4);
            Instrument lead;
            lead.wave = Waveform::Square;
            lead.pulseWidth = 0.25f;
            lead.envelope = {0.005f, 0.08f, 0.55f, 0.08f};
            lead.gain = 0.16f;
            lead.vibratoDepth = 0.12f;
            lead.vibratoRate = 6.0f;
            lead.delaySend = 0.25f;
            Instrument bass;
            bass.wave = Waveform::Triangle;
            bass.envelope = {0.004f, 0.05f, 0.8f, 0.05f};
            bass.gain = 0.3f;
            Instrument drum;
            drum.wave = Waveform::Noise;
            drum.envelope = {0.001f, 0.07f, 0.0f, 0.02f};
            drum.filter = FilterMode::HighPass;
            drum.cutoff = 3000.0f;
            drum.gain = 0.12f;
            Instrument kick;
            kick.wave = Waveform::Sine;
            kick.envelope = {0.001f, 0.12f, 0.0f, 0.02f};
            kick.pitchSlide = -48.0f;
            kick.gain = 0.4f;
            const int iLead = song.addInstrument(lead);
            const int iBass = song.addInstrument(bass);
            const int iHat = song.addInstrument(drum);
            const int iKick = song.addInstrument(kick);

            const int melodyA = song.pattern("E5 . G5 . | C6 - B5 . | A5 . G5 . | E5 - - . "
                                             "F5 . A5 . | G5 - E5 . | D5 . E5 . | C5 - - . ");
            const int melodyB = song.pattern("C5 . E5 G5 | C6 . G5 . | A5 . F5 . | C6 - - . "
                                             "B4 . D5 G5 | B5 . A5 . | G5 . F5 . | E5 - - . ");
            const int bassA = song.pattern("C3 . C3 . | G2 . G2 . | A2 . A2 . | E2 . E2 . "
                                           "F2 . F2 . | C3 . C3 . | G2 . G2 . | C3 . G2 . ");
            const int hats = song.pattern(". . x . | . . x . | . . x . | . . x x", 1.0f, 96.0f);
            const int kicks = song.pattern("x . . . | x . . . | x . . . | x . x .", 1.0f, 52.0f);
            song.addTrack(iLead, {melodyA, melodyB});
            song.addTrack(iBass, {bassA, bassA});
            song.addTrack(iHat, {hats, hats, hats, hats});
            song.addTrack(iKick, {kicks, kicks, kicks, kicks});
            return song;
        }

        Song sandboxAmbient() {
            Song song(64.0f, 2);
            Instrument pad;
            pad.wave = Waveform::Triangle;
            pad.envelope = {1.2f, 0.8f, 0.6f, 2.0f};
            pad.filter = FilterMode::LowPass;
            pad.cutoff = 1200.0f;
            pad.gain = 0.12f;
            pad.vibratoDepth = 0.05f;
            pad.vibratoRate = 0.3f;
            pad.delaySend = 0.35f;
            Instrument bell;
            bell.wave = Waveform::Sine;
            bell.envelope = {0.005f, 0.6f, 0.0f, 0.6f};
            bell.gain = 0.1f;
            bell.delaySend = 0.6f;
            const int iPadLow = song.addInstrument(pad);
            Instrument padHigh = pad;
            padHigh.pan = 0.3f;
            padHigh.gain = 0.08f;
            const int iPadHigh = song.addInstrument(padHigh);
            const int iBell = song.addInstrument(bell);

            // Cuatro acordes lentos (Do, La menor, Fa, Sol) de 8 pasos cada uno
            const int low =
                song.pattern("C3 - - - - - - - | A2 - - - - - - - | F2 - - - - - - - | G2 - - - - - - -");
            const int high =
                song.pattern("E4 - - - G4 - - - | E4 - - - C4 - - - | A3 - - - C4 - - - | B3 - - - D4 - - -");
            const int bells =
                song.pattern(". . . G5 . . . . | . . E5 . . . . . | . . . . . C6 . . | . D5 . . . . . .");
            song.addTrack(iPadLow, {low});
            song.addTrack(iPadHigh, {high});
            song.addTrack(iBell, {bells});
            return song;
        }
    }
}
