#include <mge/synth/Mixer.h>

#include <algorithm>
#include <cmath>

namespace mge {

    namespace {
        constexpr double kTwoPi = 6.283185307179586;
        constexpr float kFadeSeconds = 0.005f; // Rampa de entrada/salida de los tonos (sin chasquidos)
        constexpr float kSqrt2 = 1.41421356f;
    }

    void panGains(float pan, float& left, float& right) {
        const float angle = (std::clamp(pan, -1.0f, 1.0f) + 1.0f) * 0.25f * 3.14159265f; // 0..pi/2
        left = std::cos(angle);
        right = std::sin(angle);
    }

    float softClip(float x) {
        constexpr float kKnee = 0.8f;
        const float magnitude = std::abs(x);
        if (magnitude <= kKnee) return x;
        const float curved = kKnee + (1.0f - kKnee) * std::tanh((magnitude - kKnee) / (1.0f - kKnee));
        return x < 0.0f ? -curved : curved;
    }

    Mixer::Mixer(int sampleRate)
        : m_sampleRate(sampleRate > 0 ? sampleRate : kDefaultSampleRate),
          m_samples(std::make_unique<Sample[]>(kMaxSamples)),
          m_voices(std::make_unique<std::array<Voice, kMaxVoices>>()),
          m_delay(kMaxDelaySeconds, m_sampleRate) {
        for (auto& volume : m_busVolume) volume.store(1.0f, std::memory_order_relaxed);
    }

    Mixer::~Mixer() = default;

    SampleId Mixer::addSample(Sample sample) {
        const std::size_t index = m_sampleCount.load(std::memory_order_relaxed);
        if (sample.empty() || sample.channels < 1 || sample.channels > 2 || index >= kMaxSamples)
            return kInvalidSample;
        m_samples[index] = std::move(sample);
        m_sampleCount.store(index + 1, std::memory_order_release); // Publica el sample antes que su id
        return static_cast<SampleId>(index);
    }

    const Sample* Mixer::sample(SampleId id) const {
        return id < m_sampleCount.load(std::memory_order_acquire) ? &m_samples[id] : nullptr;
    }

    bool Mixer::enqueue(const Command& command) {
        if (m_commands.push(command)) return true;
        m_dropped.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    bool Mixer::playTone(const ToneDesc& tone) {
        Command command;
        command.type = CommandType::PlayTone;
        command.tone = tone;
        return enqueue(command);
    }

    bool Mixer::playSample(SampleId id, float gain, float pan, AudioBus bus) {
        if (!sample(id)) return false;
        Command command;
        command.type = CommandType::PlaySample;
        command.sample = id;
        command.gain = gain;
        command.pan = pan;
        command.bus = bus;
        return enqueue(command);
    }

    VoiceHandle Mixer::playNote(const Instrument& instrument, float frequency, float velocity, float duration,
                                AudioBus bus) {
        if (frequency <= 0.0f) return {};
        Command command;
        command.type = CommandType::PlayNote;
        command.instrument = instrument;
        command.frequency = frequency;
        command.gain = std::clamp(velocity, 0.0f, 1.0f);
        command.duration = duration;
        command.bus = bus;
        command.noteId = m_nextNoteId++;
        if (m_nextNoteId == 0) m_nextNoteId = 1;
        return enqueue(command) ? VoiceHandle{command.noteId} : VoiceHandle{};
    }

    bool Mixer::releaseNote(VoiceHandle note) {
        if (!note) return false;
        Command command;
        command.type = CommandType::Release;
        command.noteId = note.id;
        return enqueue(command);
    }

    bool Mixer::stopNote(VoiceHandle note) {
        if (!note) return false;
        Command command;
        command.type = CommandType::Stop;
        command.noteId = note.id;
        return enqueue(command);
    }

    bool Mixer::stopAll() {
        return enqueue(Command{});
    }

    bool Mixer::setDelay(const DelaySettings& settings) {
        Command command;
        command.type = CommandType::SetDelay;
        command.delay = settings;
        return enqueue(command);
    }

    void Mixer::setMasterVolume(float volume) {
        m_masterVolume.store(std::clamp(volume, 0.0f, 1.0f), std::memory_order_relaxed);
    }

    void Mixer::setBusVolume(AudioBus bus, float volume) {
        m_busVolume[std::size_t(bus)].store(std::clamp(volume, 0.0f, 1.0f), std::memory_order_relaxed);
    }

    MixerStats Mixer::stats() const {
        MixerStats stats;
        stats.activeVoices = m_activeVoices.load(std::memory_order_relaxed);
        stats.droppedCommands = m_dropped.load(std::memory_order_relaxed);
        stats.stolenVoices = m_stolen.load(std::memory_order_relaxed);
        return stats;
    }

    // Una libre; si no hay, la más baja de las que se están apagando; si no, la más vieja
    Mixer::Voice& Mixer::allocateVoice() {
        Voice* oldest = nullptr;
        Voice* quietest = nullptr;
        for (Voice& voice : *m_voices) {
            if (voice.kind == VoiceKind::Free) return voice;
            if (!oldest || voice.startOrder < oldest->startOrder) oldest = &voice;
            if (voice.kind == VoiceKind::Note && voice.envelope.releasing() &&
                (!quietest || voice.envelope.level() < quietest->envelope.level()))
                quietest = &voice;
        }
        m_stolen.fetch_add(1, std::memory_order_relaxed);
        return quietest ? *quietest : *oldest;
    }

    void Mixer::startNote(const Command& command) {
        const Instrument& in = command.instrument;
        Voice& voice = allocateVoice();
        voice.kind = VoiceKind::Note;
        voice.bus = command.bus;
        voice.noteId = command.noteId;
        voice.frame = 0;
        voice.totalFrames =
            command.duration > 0.0f
                ? std::max<std::size_t>(1, std::size_t(command.duration * float(m_sampleRate)))
                : 0;
        voice.oscillator.setWaveform(in.wave);
        voice.oscillator.setPulseWidth(in.pulseWidth);
        voice.oscillator.reset(command.noteId);
        voice.oscillator.setFrequency(command.frequency, m_sampleRate);
        voice.envelope.start(in.envelope, m_sampleRate);
        voice.filter.set(in.filter, in.cutoff, in.resonance, m_sampleRate);
        voice.filter.reset();
        voice.baseFrequency = command.frequency;
        voice.vibratoDepth = in.vibratoDepth;
        voice.vibratoRate = in.vibratoRate;
        voice.pitchSlide = in.pitchSlide;
        voice.delaySend = std::clamp(in.delaySend, 0.0f, 1.0f);
        panGains(in.pan, voice.gainLeft, voice.gainRight);
        const float gain = in.gain * command.gain * kSqrt2; // Centro = ganancia del instrumento
        voice.gainLeft *= gain;
        voice.gainRight *= gain;
        voice.startOrder = m_nextOrder++;
    }

    void Mixer::applyCommands() {
        Command command;
        while (m_commands.pop(command)) {
            switch (command.type) {
            case CommandType::StopAll:
                for (Voice& voice : *m_voices) voice.kind = VoiceKind::Free;
                break;
            case CommandType::SetDelay: m_delay.setSettings(command.delay); break;
            case CommandType::PlayNote: startNote(command); break;
            case CommandType::Release:
            case CommandType::Stop:
                for (Voice& voice : *m_voices) {
                    if (voice.kind != VoiceKind::Note || voice.noteId != command.noteId) continue;
                    if (command.type == CommandType::Stop)
                        voice.kind = VoiceKind::Free;
                    else
                        voice.envelope.noteOff();
                }
                break;
            case CommandType::PlayTone: {
                const ToneDesc& tone = command.tone;
                if (tone.duration <= 0.0f || tone.frequency <= 0.0f) break;
                Voice& voice = allocateVoice();
                voice.kind = VoiceKind::Tone;
                voice.bus = AudioBus::Effects;
                voice.noteId = 0;
                voice.frame = 0;
                voice.phase = 0.0;
                voice.phaseStep = kTwoPi * double(tone.frequency) / double(m_sampleRate);
                voice.totalFrames = std::size_t(double(tone.duration) * m_sampleRate);
                voice.fadeFrames =
                    std::min(std::size_t(kFadeSeconds * float(m_sampleRate)), voice.totalFrames / 2);
                voice.delaySend = 0.0f;
                panGains(tone.pan, voice.gainLeft, voice.gainRight);
                voice.gainLeft *= tone.gain;
                voice.gainRight *= tone.gain;
                voice.startOrder = m_nextOrder++;
                break;
            }
            case CommandType::PlaySample: {
                const Sample* data = sample(command.sample);
                if (!data) break;
                Voice& voice = allocateVoice();
                voice.kind = VoiceKind::Sample;
                voice.bus = command.bus;
                voice.noteId = 0;
                voice.frame = 0;
                voice.sample = data;
                voice.totalFrames = data->frameCount();
                voice.delaySend = 0.0f;
                panGains(command.pan, voice.gainLeft, voice.gainRight);
                // Pan de potencia constante con el centro a 1 (un sample mono al centro suena como el
                // original)
                voice.gainLeft *= command.gain * kSqrt2;
                voice.gainRight *= command.gain * kSqrt2;
                voice.startOrder = m_nextOrder++;
                break;
            }
            }
        }
    }

    void Mixer::mixVoice(Voice& voice, float* out, std::size_t frames, float busGain) {
        const float left = voice.gainLeft * busGain;
        const float right = voice.gainRight * busGain;
        switch (voice.kind) {
        case VoiceKind::Free: return;
        case VoiceKind::Tone:
            for (std::size_t i = 0; i < frames && voice.frame < voice.totalFrames; i++, voice.frame++) {
                float envelope = 1.0f;
                if (voice.fadeFrames > 0) {
                    const std::size_t fromEnd = voice.totalFrames - voice.frame;
                    if (voice.frame < voice.fadeFrames)
                        envelope = float(voice.frame) / float(voice.fadeFrames);
                    else if (fromEnd < voice.fadeFrames)
                        envelope = float(fromEnd) / float(voice.fadeFrames);
                }
                const float value = float(std::sin(voice.phase)) * envelope;
                voice.phase += voice.phaseStep;
                if (voice.phase >= kTwoPi) voice.phase -= kTwoPi;
                out[i * 2] += value * left;
                out[i * 2 + 1] += value * right;
            }
            if (voice.frame >= voice.totalFrames) voice.kind = VoiceKind::Free;
            return;
        case VoiceKind::Sample: {
            const Sample& s = *voice.sample;
            const float* data = s.data.data();
            for (std::size_t i = 0; i < frames && voice.frame < voice.totalFrames; i++, voice.frame++) {
                const float l = data[voice.frame * std::size_t(s.channels)];
                const float r = s.channels == 2 ? data[voice.frame * 2 + 1] : l;
                out[i * 2] += l * left;
                out[i * 2 + 1] += r * right;
            }
            if (voice.frame >= voice.totalFrames) voice.kind = VoiceKind::Free;
            return;
        }
        case VoiceKind::Note: {
            const bool modulated = voice.vibratoDepth != 0.0f || voice.pitchSlide != 0.0f;
            const float rate = float(m_sampleRate);
            for (std::size_t i = 0; i < frames; i++, voice.frame++) {
                if (voice.totalFrames > 0 && voice.frame == voice.totalFrames) voice.envelope.noteOff();
                if (modulated) {
                    const float t = float(voice.frame) / rate;
                    const float semitones = voice.pitchSlide * t +
                                            voice.vibratoDepth * std::sin(6.2831853f * voice.vibratoRate * t);
                    voice.oscillator.setFrequency(voice.baseFrequency * std::exp2(semitones / 12.0f),
                                                  m_sampleRate);
                }
                const float value = voice.filter.process(voice.oscillator.next()) * voice.envelope.next();
                out[i * 2] += value * left;
                out[i * 2 + 1] += value * right;
                if (voice.delaySend > 0.0f) {
                    m_send[i * 2] += value * left * voice.delaySend;
                    m_send[i * 2 + 1] += value * right * voice.delaySend;
                }
                if (voice.envelope.finished()) {
                    voice.kind = VoiceKind::Free;
                    break;
                }
            }
            return;
        }
        }
    }

    void Mixer::renderBlock(float* out, std::size_t frames) {
        std::fill(out, out + frames * kAudioChannels, 0.0f);
        std::fill(m_send.begin(), m_send.begin() + std::ptrdiff_t(frames * 2), 0.0f);
        std::array<float, kAudioBusCount> busGain{};
        for (std::size_t b = 0; b < kAudioBusCount; b++)
            busGain[b] = m_busVolume[b].load(std::memory_order_relaxed);
        for (Voice& voice : *m_voices)
            if (voice.kind != VoiceKind::Free) mixVoice(voice, out, frames, busGain[std::size_t(voice.bus)]);

        // Eco: entra el envío, sale la parte con eco (aunque no haya envío, la cola del eco sigue sonando)
        if (m_delay.settings().mix > 0.0f) {
            for (std::size_t i = 0; i < frames; i++) {
                float echoLeft = 0.0f, echoRight = 0.0f;
                m_delay.process(m_send[i * 2], m_send[i * 2 + 1], echoLeft, echoRight);
                out[i * 2] += echoLeft;
                out[i * 2 + 1] += echoRight;
            }
        }

        // Volumen general: rampa lineal del valor actual al pedido a lo largo del bloque, y saturación suave
        const float target =
            m_muted.load(std::memory_order_relaxed) ? 0.0f : m_masterVolume.load(std::memory_order_relaxed);
        const float step = (target - m_currentGain) / float(frames);
        for (std::size_t i = 0; i < frames; i++) {
            m_currentGain += step;
            out[i * 2] = softClip(out[i * 2] * m_currentGain);
            out[i * 2 + 1] = softClip(out[i * 2 + 1] * m_currentGain);
        }
        m_currentGain = target;
    }

    void Mixer::render(float* out, std::size_t frames) {
        applyCommands();
        for (std::size_t done = 0; done < frames;) {
            const std::size_t block = std::min(kBlockFrames, frames - done);
            renderBlock(out + done * kAudioChannels, block);
            done += block;
        }
        std::uint32_t active = 0;
        for (const Voice& voice : *m_voices) active += voice.kind != VoiceKind::Free ? 1u : 0u;
        m_activeVoices.store(active, std::memory_order_relaxed);
    }
}
