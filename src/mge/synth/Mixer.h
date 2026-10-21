#pragma once

#include <mge/core/SpscQueue.h>
#include <mge/synth/Sample.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace mge {

    // Un tono senoidal simple (#46): la prueba mínima de que sale sonido sin archivos
    struct ToneDesc {
        float frequency = 440.0f; // Hz
        float duration = 0.5f;    // Segundos
        float gain = 0.5f;        // 0..1
        float pan = 0.0f;         // -1 izquierda .. 1 derecha
    };

    struct MixerStats {
        std::uint32_t activeVoices = 0;
        std::uint64_t droppedCommands = 0; // La cola estaba llena
        std::uint64_t stolenVoices = 0;    // No había voces libres: se reemplazó la más vieja
    };

    // Mezclador del motor (#46), headless: el AudioModule lo conecta al dispositivo, los tests y la galería
    // lo usan sin dispositivo.
    // - Hilo del juego: playTone/playSample/stopAll/addSample/setMasterVolume/setMuted. Encolan comandos en
    //   una cola sin locks (SpscQueue): un solo hilo del juego puede llamarlos.
    // - Hilo de audio: render(). No reserva memoria, no bloquea ni escribe en el log.
    // Los samples registrados no cambian ni se liberan hasta destruir el mixer (el hilo de audio los lee por
    // id).
    class Mixer {
    public:
        static constexpr std::size_t kMaxVoices = 32;
        static constexpr std::size_t kMaxSamples = 256;
        static constexpr std::size_t kCommandCapacity = 256;

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

        // false = la cola de comandos estaba llena (se cuenta en stats().droppedCommands)
        bool playTone(const ToneDesc& tone);
        bool playSample(SampleId id, float gain = 1.0f, float pan = 0.0f);
        bool stopAll();

        // Volumen general 0..1 (se aplica suavizado, sin chasquidos) y silencio
        void setMasterVolume(float volume);
        float masterVolume() const { return m_masterVolume.load(std::memory_order_relaxed); }
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
            StopAll
        };
        struct Command {
            CommandType type = CommandType::StopAll;
            ToneDesc tone;
            SampleId sample = kInvalidSample;
            float gain = 1.0f;
            float pan = 0.0f;
        };

        enum class VoiceKind : std::uint8_t {
            Free,
            Tone,
            Sample
        };
        struct Voice {
            VoiceKind kind = VoiceKind::Free;
            std::uint64_t startOrder = 0; // Para robar la más vieja
            float gainLeft = 0.0f;
            float gainRight = 0.0f;
            // Tono
            double phase = 0.0;
            double phaseStep = 0.0;
            std::size_t frame = 0;
            std::size_t totalFrames = 0;
            std::size_t fadeFrames = 0;
            // Sample
            const Sample* sample = nullptr;
        };

        bool enqueue(const Command& command);
        void applyCommands();
        Voice& allocateVoice();
        void mixVoice(Voice& voice, float* out, std::size_t frames);

        int m_sampleRate;
        SpscQueue<Command, kCommandCapacity> m_commands;
        std::atomic<std::uint64_t> m_dropped{0};

        // Banco de samples: tamaño fijo, se escribe en el hilo del juego antes de publicar el id
        std::unique_ptr<Sample[]> m_samples;
        std::atomic<std::size_t> m_sampleCount{0};

        // Estado del hilo de audio
        std::array<Voice, kMaxVoices> m_voices{};
        std::uint64_t m_nextOrder = 0;
        float m_currentGain = 1.0f; // Ganancia aplicada (persigue al objetivo de a poco)
        std::atomic<std::uint32_t> m_activeVoices{0};
        std::atomic<std::uint64_t> m_stolen{0};

        std::atomic<float> m_masterVolume{1.0f};
        std::atomic<bool> m_muted{false};
    };

    // Ganancias izquierda/derecha de potencia constante para pan en [-1, 1]
    void panGains(float pan, float& left, float& right);
}
