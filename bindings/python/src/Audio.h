#pragma once

#include "Common.h"

#include <memory>
#include <string>

namespace mge::python {

    class PyApplication;

    enum class PySfxPreset {
        Jump = 0,
        Coin = 1,
        Hurt = 2,
        Explosion = 3,
        PowerUp = 4,
        Click = 5,
    };

    struct PySfxParams {
        int waveType = 0;
        float attackTime = 0.0f;
        float sustainTime = 0.1f;
        float sustainPunch = 0.0f;
        float decayTime = 0.2f;
        float startFrequency = 440.0f;
        float minFrequency = 0.0f;
        float slide = 0.0f;
        float deltaSlide = 0.0f;
        float squareDuty = 0.5f;
        float dutySweep = 0.0f;
        float vibratoDepth = 0.0f;
        float vibratoSpeed = 0.0f;
        float changeAmount = 0.0f;
        float changeSpeed = 0.0f;
        float lpFilterCutoff = 1.0f;
        float lpFilterSweep = 0.0f;
        float lpFilterResonance = 0.0f;
        float hpFilterCutoff = 0.0f;
        float hpFilterSweep = 0.0f;
        float phaserOffset = 0.0f;
        float phaserSweep = 0.0f;
        float repeatSpeed = 0.0f;
    };

    class PySample {
    public:
        PySample() = default;
        explicit PySample(mge::Sample sample) : m_sample(std::move(sample)) {}

        size_t frameCount() const { return m_sample.frameCount(); }
        float rms() const { return m_sample.empty() ? 0.0f : mge::analysis::rms(m_sample.data); }
        int channels() const { return m_sample.channels; }
        int sampleRate() const { return mge::kDefaultSampleRate; }
        bool isAlive() const { return !m_sample.empty(); }
        const mge::Sample& sample() const { return m_sample; }

        void destroy() {
            m_sample = mge::Sample{};
            m_sampleId = mge::kInvalidSample;
        }

        static std::shared_ptr<PySample> createSfx(PySfxPreset preset, uint32_t seed = 0,
                                                   mge::Application* app = nullptr);
        static std::shared_ptr<PySample> createSfxCustom(const PySfxParams& params,
                                                         mge::Application* app = nullptr);
        static std::shared_ptr<PySample> loadWav(const std::string& path, mge::Application* app = nullptr);

    private:
        friend class PyAudio;
        mge::Sample m_sample;
        mge::SampleId m_sampleId = mge::kInvalidSample;
    };

    class PyAudio {
    public:
        explicit PyAudio(PyApplication* app) : m_app(app) {}

        uint64_t play(const std::shared_ptr<PySample>& sample, float gain = 1.0f, float pan = 0.0f);
        uint64_t playSfx(PySfxPreset preset, uint32_t seed = 0, float gain = 1.0f);
        void playSong(const std::string& songName, float fadeSeconds = 0.0f);
        void stopSong(float fadeSeconds = 0.0f);

        void setMasterVolume(float volume);
        void setSfxVolume(float volume);
        void setMusicVolume(float volume);
        void setMute(bool mute);

        std::shared_ptr<PySample> createSfx(PySfxPreset preset, uint32_t seed = 0);
        std::shared_ptr<PySample> createSfxCustom(const PySfxParams& params);
        std::shared_ptr<PySample> loadWav(const std::string& path);

    private:
        PyApplication* m_app = nullptr;
    };

} // namespace mge::python
