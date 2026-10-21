#include <mge/synth/Mixer.h>

#include <algorithm>
#include <cmath>

namespace mge {

    namespace {
        constexpr double kTwoPi = 6.283185307179586;
        constexpr float kFadeSeconds = 0.005f; // Rampa de entrada/salida de los tonos (sin chasquidos)
    }

    void panGains(float pan, float& left, float& right) {
        const float angle = (std::clamp(pan, -1.0f, 1.0f) + 1.0f) * 0.25f * 3.14159265f; // 0..pi/2
        left = std::cos(angle);
        right = std::sin(angle);
    }

    Mixer::Mixer(int sampleRate)
        : m_sampleRate(sampleRate > 0 ? sampleRate : kDefaultSampleRate),
          m_samples(std::make_unique<Sample[]>(kMaxSamples)) {}

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

    bool Mixer::playSample(SampleId id, float gain, float pan) {
        if (!sample(id)) return false;
        Command command;
        command.type = CommandType::PlaySample;
        command.sample = id;
        command.gain = gain;
        command.pan = pan;
        return enqueue(command);
    }

    bool Mixer::stopAll() {
        return enqueue(Command{});
    }

    void Mixer::setMasterVolume(float volume) {
        m_masterVolume.store(std::clamp(volume, 0.0f, 1.0f), std::memory_order_relaxed);
    }

    MixerStats Mixer::stats() const {
        MixerStats stats;
        stats.activeVoices = m_activeVoices.load(std::memory_order_relaxed);
        stats.droppedCommands = m_dropped.load(std::memory_order_relaxed);
        stats.stolenVoices = m_stolen.load(std::memory_order_relaxed);
        return stats;
    }

    Mixer::Voice& Mixer::allocateVoice() {
        Voice* oldest = &m_voices[0];
        for (Voice& voice : m_voices) {
            if (voice.kind == VoiceKind::Free) return voice;
            if (voice.startOrder < oldest->startOrder) oldest = &voice;
        }
        m_stolen.fetch_add(1, std::memory_order_relaxed);
        return *oldest;
    }

    void Mixer::applyCommands() {
        Command command;
        while (m_commands.pop(command)) {
            switch (command.type) {
            case CommandType::StopAll:
                for (Voice& voice : m_voices) voice.kind = VoiceKind::Free;
                break;
            case CommandType::PlayTone: {
                const ToneDesc& tone = command.tone;
                if (tone.duration <= 0.0f || tone.frequency <= 0.0f) break;
                Voice& voice = allocateVoice();
                voice = Voice{};
                voice.kind = VoiceKind::Tone;
                voice.phaseStep = kTwoPi * double(tone.frequency) / double(m_sampleRate);
                voice.totalFrames = std::size_t(double(tone.duration) * m_sampleRate);
                voice.fadeFrames =
                    std::min(std::size_t(kFadeSeconds * float(m_sampleRate)), voice.totalFrames / 2);
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
                voice = Voice{};
                voice.kind = VoiceKind::Sample;
                voice.sample = data;
                voice.totalFrames = data->frameCount();
                panGains(command.pan, voice.gainLeft, voice.gainRight);
                // Pan de potencia constante con el centro a 1 (un sample mono al centro suena como el
                // original)
                const float center = 1.41421356f;
                voice.gainLeft *= command.gain * center;
                voice.gainRight *= command.gain * center;
                voice.startOrder = m_nextOrder++;
                break;
            }
            }
        }
    }

    void Mixer::mixVoice(Voice& voice, float* out, std::size_t frames) {
        if (voice.kind == VoiceKind::Tone) {
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
                out[i * 2] += value * voice.gainLeft;
                out[i * 2 + 1] += value * voice.gainRight;
            }
        } else if (voice.kind == VoiceKind::Sample) {
            const Sample& s = *voice.sample;
            const float* data = s.data.data();
            for (std::size_t i = 0; i < frames && voice.frame < voice.totalFrames; i++, voice.frame++) {
                const float left = data[voice.frame * std::size_t(s.channels)];
                const float right = s.channels == 2 ? data[voice.frame * 2 + 1] : left;
                out[i * 2] += left * voice.gainLeft;
                out[i * 2 + 1] += right * voice.gainRight;
            }
        }
        if (voice.frame >= voice.totalFrames) voice.kind = VoiceKind::Free;
    }

    void Mixer::render(float* out, std::size_t frames) {
        std::fill(out, out + frames * kAudioChannels, 0.0f);
        applyCommands();
        std::uint32_t active = 0;
        for (Voice& voice : m_voices) {
            if (voice.kind == VoiceKind::Free) continue;
            mixVoice(voice, out, frames);
            if (voice.kind != VoiceKind::Free) active++;
        }
        m_activeVoices.store(active, std::memory_order_relaxed);

        // Volumen general: rampa lineal del valor actual al pedido a lo largo del bloque
        const float target =
            m_muted.load(std::memory_order_relaxed) ? 0.0f : m_masterVolume.load(std::memory_order_relaxed);
        const float step = frames > 0 ? (target - m_currentGain) / float(frames) : 0.0f;
        for (std::size_t i = 0; i < frames; i++) {
            m_currentGain += step;
            out[i * 2] = std::clamp(out[i * 2] * m_currentGain, -1.0f, 1.0f);
            out[i * 2 + 1] = std::clamp(out[i * 2 + 1] * m_currentGain, -1.0f, 1.0f);
        }
        m_currentGain = target;
    }
}
