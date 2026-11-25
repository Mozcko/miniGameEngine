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

    // Simple sine tone (#46): minimal proof that sound works with no files
    struct ToneDesc {
        float frequency = 440.0f; // Hz
        float duration = 0.5f;    // Seconds
        float gain = 0.5f;        // 0..1
        float pan = 0.0f;         // -1 left .. 1 right
    };

    // Volume groups: effects (tones, notes, samples by default) and music
    enum class AudioBus : std::uint8_t {
        Effects,
        Music
    };
    constexpr std::size_t kAudioBusCount = 2;

    // Identifies a playing note so it can be released or stopped (0 = none)
    struct VoiceHandle {
        std::uint32_t id = 0;
        explicit operator bool() const { return id != 0; }
    };

    using SongId = std::uint32_t;
    constexpr SongId kInvalidSong = 0xFFFFFFFFu;

    struct MixerStats {
        std::uint32_t activeVoices = 0;
        std::uint64_t droppedCommands = 0; // Command queue was full
        std::uint64_t stolenVoices = 0;    // No free voices: replaced an active one
    };

    // Engine mixer (#46, #47), headless: AudioModule connects it to the playback device; tests and the
    // gallery run it without a device.
    // - Game thread: play*/release/stop/addSample/volumes. Pushes commands into a lock-free queue
    //   (SpscQueue): only a single game thread may call them.
    // - Audio thread: render(). No memory allocations, no locks, no logging.
    // Registered samples are immutable and stay alive until the mixer is destroyed (the audio thread reads
    // them by id).
    class Mixer {
    public:
        static constexpr std::size_t kMaxVoices = 32;
        static constexpr std::size_t kMaxSamples = 256;
        static constexpr std::size_t kCommandCapacity = 256;
        static constexpr std::size_t kBlockFrames = 256; // render() processes chunks of this size
        static constexpr float kMaxDelaySeconds = 2.0f;
        static constexpr std::size_t kMaxSongs = 16;

        explicit Mixer(int sampleRate = kDefaultSampleRate);
        ~Mixer();
        Mixer(const Mixer&) = delete;
        Mixer& operator=(const Mixer&) = delete;

        int sampleRate() const { return m_sampleRate; }

        // --- Game thread ---
        // Registers a sound (already at sampleRate()). Returns kInvalidSample if empty or bank is full
        SampleId addSample(Sample sample);
        const Sample* sample(SampleId id) const;
        std::size_t sampleCount() const { return m_sampleCount.load(std::memory_order_acquire); }

        // false / empty handle = command queue was full (counted in stats().droppedCommands)
        bool playTone(const ToneDesc& tone);
        bool playSample(SampleId id, float gain = 1.0f, float pan = 0.0f, AudioBus bus = AudioBus::Effects);
        // Plays a note on the instrument at frequency Hz (midiToHz for MIDI notes). duration: seconds until
        // release (then the release stage plays); <= 0 = held until releaseNote
        VoiceHandle playNote(const Instrument& instrument, float frequency, float velocity = 1.0f,
                             float duration = 0.25f, AudioBus bus = AudioBus::Effects);
        bool releaseNote(VoiceHandle note); // Enters release (fades out via its envelope)
        bool stopNote(VoiceHandle note);    // Cuts off immediately
        bool stopAll();                     // Voices and music
        bool setDelay(const DelaySettings& settings);

        // Music (#49): register a song (compiled here; invalid → kInvalidSong) and play it in a loop on the
        // Music bus. Switching songs with fadeSeconds > 0 performs a crossfade
        SongId addSong(const Song& song);
        bool playSong(SongId song, float fadeSeconds = 0.0f);
        bool stopMusic(float fadeSeconds = 0.0f);
        // Current step of the playing song (-1 = none). For tests and tools
        int musicStep() const { return m_musicStep.load(std::memory_order_relaxed); }

        // Master and per-bus volume 0..1 (master is smoothed to avoid clicks) and mute
        void setMasterVolume(float volume);
        float masterVolume() const { return m_masterVolume.load(std::memory_order_relaxed); }
        void setBusVolume(AudioBus bus, float volume);
        float busVolume(AudioBus bus) const {
            return m_busVolume[std::size_t(bus)].load(std::memory_order_relaxed);
        }
        void setMuted(bool muted) { m_muted.store(muted, std::memory_order_relaxed); }
        bool muted() const { return m_muted.load(std::memory_order_relaxed); }

        MixerStats stats() const;

        // --- Audio thread ---
        // Writes frames × 2 floats (interleaved stereo) into out
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
            SampleId sample = kInvalidSample; // Or the song (PlaySong)
            std::uint32_t noteId = 0;
            float frequency = 0.0f;
            float gain = 1.0f; // Sample gain or note velocity
            float pan = 0.0f;
            float duration = 0.0f; // Or fade duration (PlaySong/StopMusic)
        };

        // Song ready for the audio thread: notes sorted in time order
        struct CompiledSong {
            std::vector<Song::Event> events;
            std::vector<Instrument> instruments;
            int lengthSteps = 0;
            int loopStart = 0;
            double framesPerStep = 0.0;
        };
        // Plays one song; two instances allow crossfading
        struct MusicPlayer {
            const CompiledSong* song = nullptr;
            double position = 0.0; // Frames since step 0
            std::size_t next = 0;  // Next note index
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
            std::uint64_t startOrder = 0; // For stealing the oldest voice
            std::uint32_t noteId = 0;
            int player = -1; // Music note: which player triggered it (for its fade gain)
            float gainLeft = 0.0f;
            float gainRight = 0.0f;
            std::size_t frame = 0;
            std::size_t totalFrames = 0; // Tone/sample: length; note: frames until release (0 = held)
            // Tone
            double phase = 0.0;
            double phaseStep = 0.0;
            std::size_t fadeFrames = 0;
            // Sample
            const Sample* sample = nullptr;
            // Note
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
        // Music: frames until the next note (or loop), trigger due notes, advance and fade
        std::size_t framesUntilMusicEvent() const;
        void triggerMusic();
        void advanceMusic(std::size_t frames);
        void stopPlayer(MusicPlayer& player, float fadeSeconds);
        // Accumulates the voice into out (frames ≤ kBlockFrames) and its echo send into m_send
        void mixVoice(Voice& voice, float* out, std::size_t frames, float busGain);
        void renderBlock(float* out, std::size_t frames);

        int m_sampleRate;
        SpscQueue<Command, kCommandCapacity> m_commands;
        std::atomic<std::uint64_t> m_dropped{0};
        std::uint32_t m_nextNoteId = 1; // Game thread

        // Sample bank: fixed size, written on the game thread before publishing the id
        std::unique_ptr<Sample[]> m_samples;
        std::atomic<std::size_t> m_sampleCount{0};
        std::unique_ptr<CompiledSong[]> m_songs; // Same as samples: fixed size, id published last
        std::atomic<std::size_t> m_songCount{0};

        // Audio thread state
        std::unique_ptr<std::array<Voice, kMaxVoices>> m_voices;
        std::uint64_t m_nextOrder = 0;
        float m_currentGain = 1.0f; // Applied gain (ramps smoothly toward target)
        Delay m_delay;
        std::array<float, kBlockFrames * 2> m_send{}; // Echo send buffer for the current block
        std::array<MusicPlayer, 2> m_players{};
        std::atomic<int> m_musicStep{-1};
        std::atomic<std::uint32_t> m_activeVoices{0};
        std::atomic<std::uint64_t> m_stolen{0};

        std::atomic<float> m_masterVolume{1.0f};
        std::array<std::atomic<float>, kAudioBusCount> m_busVolume{};
        std::atomic<bool> m_muted{false};
    };

    // Constant-power left/right gains for pan in [-1, 1]
    void panGains(float pan, float& left, float& right);
    // Soft clipping: linear up to 0.8, then curves smoothly toward ±1 instead of hard clipping
    float softClip(float x);
}
