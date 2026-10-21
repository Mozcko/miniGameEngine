#pragma once

#include <mge/synth/Instrument.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace mge {

    // Una nota dentro de un patrón (en pasos: un paso = 1/stepsPerBeat de tiempo)
    struct NoteEvent {
        int step = 0;
        float note = 60.0f; // MIDI (60 = Do central)
        float velocity = 1.0f;
        int length = 1; // Pasos
    };

    // Unos compases de notas para un instrumento
    struct Pattern {
        int steps = 16;
        std::vector<NoteEvent> notes;
    };

    // Lee un patrón escrito como en un tracker (#49). Un paso por palabra, separadas por espacios:
    //   "C4"  "F#3" "Bb2"  nota (MIDI: C4 = 60)      "."  silencio
    //   "-"   sostiene la nota anterior un paso más   "x"  la nota por defecto (percusión)
    //   "|"   se ignora (marca de compás, solo para leer)
    // Ej: "C3 . G2 . | C3 - - . "  → 8 pasos. nullopt (y el motivo) si hay algo que no se entiende
    std::optional<Pattern> parsePattern(std::string_view text, float velocity = 1.0f,
                                        float defaultNote = 60.0f, std::string* error = nullptr);
    // "C4" → 60, "A4" → 69, "C#3"/"Db3" → 49. nullopt si no es una nota
    std::optional<float> noteFromName(std::string_view name);

    // Una canción definida en C++ (#49): tempo, instrumentos, patrones y pistas (cada pista toca una lista de
    // patrones, uno detrás de otro, con un instrumento). La canción dura lo que la pista más larga y se
    // repite desde loopStartStep. Se registra en el mezclador (Mixer::addSong) y no cambia más.
    class Song {
    public:
        explicit Song(float bpm = 120.0f, int stepsPerBeat = 4) : m_bpm(bpm), m_stepsPerBeat(stepsPerBeat) {}

        int addInstrument(const Instrument& instrument);
        int addPattern(Pattern pattern);
        // Atajo: parsePattern + addPattern. -1 si el texto tiene errores (ver error())
        int pattern(std::string_view text, float velocity = 1.0f, float defaultNote = 60.0f);
        // Una pista: instrumento + orden de patrones. gain extra de la pista
        void addTrack(int instrument, std::vector<int> patterns, float gain = 1.0f);
        void setLoopStart(int step) { m_loopStart = step; }

        float bpm() const { return m_bpm; }
        int stepsPerBeat() const { return m_stepsPerBeat; }
        float secondsPerStep() const { return 60.0f / (m_bpm * float(m_stepsPerBeat)); }
        int lengthSteps() const; // La pista más larga
        int loopStart() const { return m_loopStart; }
        // Vacío si la canción es válida (índices bien, tempo positivo, algo para tocar)
        std::string error() const;
        bool valid() const { return error().empty(); }

        // Todas las notas en orden de tiempo, para el reproductor
        struct Event {
            int step = 0;
            int instrument = 0;
            float frequency = 0.0f;
            float velocity = 1.0f;
            float seconds = 0.0f; // Cuánto se sostiene antes del release
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

    // Canciones de ejemplo hechas con el secuenciador (las usan el plataformero, el sandbox y la galería)
    namespace songs {
        Song platformerTheme(); // Chiptune alegre: cuadrada, bajo triangular, batería de ruido
        Song sandboxAmbient();  // Tranquila: pads suaves con filtro y eco, notas sueltas
    }
}
