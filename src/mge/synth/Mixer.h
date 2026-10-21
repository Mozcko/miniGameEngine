#pragma once

#include <mge/core/SpscQueue.h>
#include <mge/synth/Delay.h>
#include <mge/synth/Instrument.h>
#include <mge/synth/Music.h>
#include <mge/synth/Sample.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace mge {

    // Un tono senoidal simple (#46): la prueba mínima de que sale sonido sin archivos
    struct ToneDesc {
        float frequency = 440.0f; // Hz
        float duration = 0.5f;    // Segundos
        float gain = 0.5f;        // 0..1
        float pan = 0.0f;         // -1 izquierda .. 1 derecha
    };

    // Grupos de volumen: efectos (tonos, notas, samples por defecto) y música
    enum class AudioBus : std::uint8_t {
        Effects,
        Music
    };
    constexpr std::size_t kAudioBusCount = 2;

    // Identifica una nota sonando, para soltarla o cortarla (0 = ninguna)
    struct VoiceHandle {
        std::uint32_t id = 0;
        explicit operator bool() const { return id != 0; }
    };

    using SongId = std::uint32_t;
    constexpr SongId kInvalidSong = 0xFFFFFFFFu;

    struct MixerStats {
        std::uint32_t activeVoices = 0;
        std::uint64_t droppedCommands = 0; // La cola estaba llena
        std::uint64_t stolenVoices = 0;    // No había voces libres: se reemplazó otra
    };

    // Mezclador del motor (#46, #47), headless: el AudioModule lo conecta al dispositivo; los tests y la
    // galería lo usan sin dispositivo.
    // - Hilo del juego: play*/release/stop/addSample/volúmenes. Encolan comandos en una cola sin locks
    //   (SpscQueue): un solo hilo del juego puede llamarlos.
    // - Hilo de audio: render(). No reserva memoria, no bloquea ni escribe en el log.
    // Los samples registrados no cambian ni se liberan hasta destruir el mixer (el hilo de audio los lee por
    // id).
    class Mixer {
    public:
        static constexpr std::size_t kMaxVoices = 32;
        static constexpr std::size_t kMaxSamples = 256;
        static constexpr std::size_t kCommandCapacity = 256;
        static constexpr std::size_t kBlockFrames = 256; // render() trabaja en tramos de este tamaño
        static constexpr float kMaxDelaySeconds = 2.0f;
        static constexpr std::size_t kMaxSongs = 16;

        explicit Mixer(int sampleRate = kDefaultSampleRate);
        ~Mixer();
        Mixer(const Mixer&) = delete;
        Mixer& operator=(const Mixer&) = delete;

        int sampleRate() const { return m_sampleRate; }

        // --- Hilo del juego ---
        // Registra un sonido (ya a sampleRate()). kInvalidSample si está vacío o el banco está lleno
        SampleId addSample(Sample sample);
        const Sample* sample(SampleId id) const;
        std::size_t sampleCount() const { return m_sampleCount.load(std::memory_order_acquire); }

        // false / handle vacío = la cola de comandos estaba llena (se cuenta en stats().droppedCommands)
        bool playTone(const ToneDesc& tone);
        bool playSample(SampleId id, float gain = 1.0f, float pan = 0.0f, AudioBus bus = AudioBus::Effects);
        // Una nota del instrumento a frequency Hz (midiToHz para notas MIDI). duration: segundos hasta
        // soltarla (después suena el release); <= 0 = sostenida hasta releaseNote
        VoiceHandle playNote(const Instrument& instrument, float frequency, float velocity = 1.0f,
                             float duration = 0.25f, AudioBus bus = AudioBus::Effects);
        bool releaseNote(VoiceHandle note); // Pasa a release (se apaga con su envolvente)
        bool stopNote(VoiceHandle note);    // Corta ya
        bool stopAll();                     // Voces y música
        bool setDelay(const DelaySettings& settings);

        // Música (#49): registrar una canción (se compila acá; inválida → kInvalidSong) y tocarla en loop en
        // el grupo Music. Cambiar de canción con fadeSeconds > 0 hace un fundido cruzado
        SongId addSong(const Song& song);
        bool playSong(SongId song, float fadeSeconds = 0.0f);
        bool stopMusic(float fadeSeconds = 0.0f);
        // Paso de la canción que está sonando (-1 = ninguna). Para tests y herramientas
        int musicStep() const { return m_musicStep.load(std::memory_order_relaxed); }

        // Volumen general y por grupo 0..1 (el general se aplica suavizado, sin chasquidos) y silencio
        void setMasterVolume(float volume);
        float masterVolume() const { return m_masterVolume.load(std::memory_order_relaxed); }
        void setBusVolume(AudioBus bus, float volume);
        float busVolume(AudioBus bus) const {
            return m_busVolume[std::size_t(bus)].load(std::memory_order_relaxed);
        }
        void setMuted(bool muted) { m_muted.store(muted, std::memory_order_relaxed); }
        bool muted() const { return m_muted.load(std::memory_order_relaxed); }

        MixerStats stats() const;

        // --- Hilo de audio ---
        // Escribe frames × 2 floats (estéreo intercalado) en out
        void render(float* out, std::size_t frames);

    private:
        enum class CommandType : std::uint8_t {
            PlayTone,
            PlaySample,
            PlayNote,
            Release,
            Stop,
            StopAll,
            SetDelay,
            PlaySong,
            StopMusic
        };
        struct Command {
            CommandType type = CommandType::StopAll;
            AudioBus bus = AudioBus::Effects;
            ToneDesc tone;
            Instrument instrument;
            DelaySettings delay;
            SampleId sample = kInvalidSample; // O la canción (PlaySong)
            std::uint32_t noteId = 0;
            float frequency = 0.0f;
            float gain = 1.0f; // Ganancia del sample o velocity de la nota
            float pan = 0.0f;
            float duration = 0.0f; // O el fundido (PlaySong/StopMusic)
        };

        // Canción lista para el hilo de audio: notas en orden de tiempo
        struct CompiledSong {
            std::vector<Song::Event> events;
            std::vector<Instrument> instruments;
            int lengthSteps = 0;
            int loopStart = 0;
            double framesPerStep = 0.0;
        };
        // Toca una canción; hay dos para el fundido cruzado
        struct MusicPlayer {
            const CompiledSong* song = nullptr;
            double position = 0.0; // Frames desde el paso 0
            std::size_t next = 0;  // Próxima nota
            float gain = 0.0f;
            float target = 0.0f;
            float fadePerFrame = 0.0f;
            bool active = false;
        };

        enum class VoiceKind : std::uint8_t {
            Free,
            Tone,
            Sample,
            Note
        };
        struct Voice {
            VoiceKind kind = VoiceKind::Free;
            AudioBus bus = AudioBus::Effects;
            std::uint64_t startOrder = 0; // Para robar la más vieja
            std::uint32_t noteId = 0;
            int player = -1; // Nota de música: qué reproductor la tocó (su volumen de fundido)
            float gainLeft = 0.0f;
            float gainRight = 0.0f;
            std::size_t frame = 0;
            std::size_t totalFrames = 0; // Tono/sample: largo; nota: frames hasta soltarla (0 = sostenida)
            // Tono
            double phase = 0.0;
            double phaseStep = 0.0;
            std::size_t fadeFrames = 0;
            // Sample
            const Sample* sample = nullptr;
            // Nota
            Oscillator oscillator;
            Envelope envelope;
            Filter filter;
            float baseFrequency = 0.0f;
            float vibratoDepth = 0.0f;
            float vibratoRate = 0.0f;
            float pitchSlide = 0.0f;
            float delaySend = 0.0f;
        };

        bool enqueue(const Command& command);
        void applyCommands();
        Voice& allocateVoice();
        void startVoice(const Instrument& instrument, float frequency, float velocity, float duration,
                        AudioBus bus, std::uint32_t noteId, int player);
        // Música: frames hasta la próxima nota (o el loop), disparar las que tocan, avanzar y fundir
        std::size_t framesUntilMusicEvent() const;
        void triggerMusic();
        void advanceMusic(std::size_t frames);
        void stopPlayer(MusicPlayer& player, float fadeSeconds);
        // Suma la voz en out (frames ≤ kBlockFrames) y su envío al eco en m_send
        void mixVoice(Voice& voice, float* out, std::size_t frames, float busGain);
        void renderBlock(float* out, std::size_t frames);

        int m_sampleRate;
        SpscQueue<Command, kCommandCapacity> m_commands;
        std::atomic<std::uint64_t> m_dropped{0};
        std::uint32_t m_nextNoteId = 1; // Hilo del juego

        // Banco de samples: tamaño fijo, se escribe en el hilo del juego antes de publicar el id
        std::unique_ptr<Sample[]> m_samples;
        std::atomic<std::size_t> m_sampleCount{0};
        std::unique_ptr<CompiledSong[]> m_songs; // Igual que los samples: fijo, se publica el id al final
        std::atomic<std::size_t> m_songCount{0};

        // Estado del hilo de audio
        std::unique_ptr<std::array<Voice, kMaxVoices>> m_voices;
        std::uint64_t m_nextOrder = 0;
        float m_currentGain = 1.0f; // Ganancia aplicada (persigue al objetivo de a poco)
        Delay m_delay;
        std::array<float, kBlockFrames * 2> m_send{}; // Lo que va al eco en el bloque actual
        std::array<MusicPlayer, 2> m_players{};
        std::atomic<int> m_musicStep{-1};
        std::atomic<std::uint32_t> m_activeVoices{0};
        std::atomic<std::uint64_t> m_stolen{0};

        std::atomic<float> m_masterVolume{1.0f};
        std::array<std::atomic<float>, kAudioBusCount> m_busVolume{};
        std::atomic<bool> m_muted{false};
    };

    // Ganancias izquierda/derecha de potencia constante para pan en [-1, 1]
    void panGains(float pan, float& left, float& right);
    // Saturación suave: lineal hasta 0.8, después se curva hacia ±1 sin cortar en seco
    float softClip(float x);
}
