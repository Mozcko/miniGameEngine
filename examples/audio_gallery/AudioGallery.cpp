// Galería de audio (#50): genera sonidos con el sintetizador del motor, sin dispositivo ni archivos de
// entrada, y los escribe en <build>/gallery/audio/*.wav para escucharlos, con un espectrograma *.png de cada
// uno (tiempo → x, frecuencia → y). Headless: corre en CI; falla si algo no se pudo escribir o salió mudo.
#include <mge/mge.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace {
    constexpr int kRate = mge::kDefaultSampleRate;

    // Espectrograma: bandas lineales de 0 a 8 kHz, una columna cada 10 ms, brillo en dB (−60..0)
    mge::Image spectrogram(const std::vector<float>& mono) {
        constexpr int kBands = 96;
        constexpr float kMaxHz = 8000.0f;
        const std::size_t hop = kRate / 100;
        const std::size_t window = 1024;
        const int columns = int(mono.size() > window ? (mono.size() - window) / hop : 0) + 1;
        mge::Image image(std::uint32_t(columns), kBands, {0.02f, 0.02f, 0.05f, 1.0f});
        for (int x = 0; x < columns; x++) {
            const std::size_t from = std::size_t(x) * hop;
            const std::size_t to = std::min(mono.size(), from + window);
            for (int band = 0; band < kBands; band++) {
                const float hz = (float(band) + 0.5f) * kMaxHz / float(kBands);
                const float amplitude = mge::analysis::toneAmplitude(mono, hz, kRate, from, to);
                const float db = 20.0f * std::log10(std::max(amplitude, 1e-6f));
                const float t = std::clamp((db + 60.0f) / 60.0f, 0.0f, 1.0f);
                image.set(x, kBands - 1 - band, {t, t * t, 0.3f + 0.7f * t * (1.0f - t), 1.0f});
            }
        }
        return image;
    }

    class AudioGallery : public mge::Application {
    public:
        AudioGallery() : mge::Application(mge::ApplicationConfig{}) {}

    protected:
        bool onInit() override {
            m_outDir = mge::executableDir() / "gallery" / "audio";
            std::filesystem::create_directories(m_outDir);

            // Las formas de onda: un arpegio corto con cada una
            for (const auto& [name, wave] : std::vector<std::pair<std::string, mge::Waveform>>{
                     {"sine", mge::Waveform::Sine},
                     {"square", mge::Waveform::Square},
                     {"saw", mge::Waveform::Saw},
                     {"triangle", mge::Waveform::Triangle},
                     {"noise", mge::Waveform::Noise},
                     {"noise_loop", mge::Waveform::NoiseLoop}}) {
                mge::Instrument in;
                in.wave = wave;
                in.envelope = {0.005f, 0.05f, 0.7f, 0.1f};
                in.gain = 0.4f;
                write("wave_" + name, 1.6f, [&](mge::Mixer& mixer, float t, int step) {
                    constexpr int kNotes[4] = {57, 61, 64, 69};
                    if (step < 4 && t >= 0.35f * float(step))
                        mixer.playNote(in, mge::midiToHz(float(kNotes[step])), 1.0f, 0.25f);
                    return step < 4 && t >= 0.35f * float(step);
                });
            }

            // ADSR: pluck, pad y percusivo
            write("adsr", 3.0f, [](mge::Mixer& mixer, float t, int step) {
                mge::Instrument in;
                in.wave = mge::Waveform::Triangle;
                in.gain = 0.5f;
                const mge::Adsr shapes[3] = {
                    {0.002f, 0.15f, 0.0f, 0.05f}, {0.4f, 0.2f, 0.7f, 0.5f}, {0.001f, 0.05f, 0.3f, 0.3f}};
                if (step >= 3 || t < float(step)) return false;
                in.envelope = shapes[step];
                mixer.playNote(in, 330.0f, 1.0f, 0.5f);
                return true;
            });

            // Filtro: la misma sierra, el corte bajando de 8 kHz a 200 Hz, con resonancia
            write("filter_sweep", 2.6f, [](mge::Mixer& mixer, float t, int step) {
                if (step >= 13 || t < 0.2f * float(step)) return false;
                mge::Instrument in;
                in.wave = mge::Waveform::Saw;
                in.envelope = {0.005f, 0.05f, 0.8f, 0.05f};
                in.filter = mge::FilterMode::LowPass;
                in.cutoff = 8000.0f * std::pow(0.735f, float(step));
                in.resonance = 0.6f;
                in.gain = 0.4f;
                mixer.playNote(in, 110.0f, 1.0f, 0.18f);
                return true;
            });

            // Eco + vibrato + deslizamiento de tono
            write("delay_vibrato_slide", 3.0f, [](mge::Mixer& mixer, float t, int step) {
                if (step == 0) {
                    mixer.setDelay({0.25f, 0.45f, 0.5f});
                    mge::Instrument lead;
                    lead.wave = mge::Waveform::Square;
                    lead.pulseWidth = 0.3f;
                    lead.envelope = {0.01f, 0.1f, 0.6f, 0.2f};
                    lead.vibratoDepth = 0.3f;
                    lead.vibratoRate = 6.0f;
                    lead.delaySend = 0.6f;
                    lead.gain = 0.35f;
                    mixer.playNote(lead, mge::midiToHz(76.0f), 1.0f, 0.6f);
                    return true;
                }
                if (step == 1 && t >= 1.2f) {
                    mge::Instrument laser;
                    laser.wave = mge::Waveform::Saw;
                    laser.envelope = {0.001f, 0.3f, 0.0f, 0.05f};
                    laser.pitchSlide = -36.0f;
                    laser.gain = 0.35f;
                    mixer.playNote(laser, 1760.0f, 1.0f, 0.4f);
                    return true;
                }
                return false;
            });

            // Efectos sfxr (#48): los equivalentes de assets/sounds y algunos al azar
            const std::vector<std::pair<std::string, mge::SfxParams>> effects = {
                {"jump", mge::sfx::jump()},           {"coin", mge::sfx::coin()},
                {"hurt", mge::sfx::hurt()},           {"explosion", mge::sfx::explosion()},
                {"power_up", mge::sfx::powerUp()},    {"click", mge::sfx::click()},
                {"random_1", mge::sfx::randomize(1)}, {"random_2", mge::sfx::randomize(2)},
                {"random_3", mge::sfx::randomize(3)}};
            for (const auto& effect : effects) {
                const mge::Sample sample = mge::renderSfx(effect.second, kRate);
                writeMono("sfx_" + effect.first, sample.data);
            }

            MGE_LOG_INFO("Gallery", m_written, " sonidos (.wav + espectrograma .png) en ", m_outDir.string());
            requestStop();
            return true;
        }

    private:
        // Corre el mezclador offline en bloques de 10 ms; script(mixer, t, paso) dispara eventos y devuelve
        // true cuando usó el paso actual
        void write(const std::string& name, float seconds,
                   const std::function<bool(mge::Mixer&, float, int)>& script) {
            mge::Mixer mixer(kRate);
            std::vector<float> stereo;
            const std::size_t block = kRate / 100;
            int step = 0;
            for (std::size_t frame = 0; frame < std::size_t(seconds * float(kRate)); frame += block) {
                while (script(mixer, float(frame) / float(kRate), step)) step++;
                const std::vector<float> part = mge::renderOffline(mixer, block);
                stereo.insert(stereo.end(), part.begin(), part.end());
            }
            if (!check(name, mge::analysis::channel(stereo, 2, 0))) return;
            if (!mge::writeWav(m_outDir / (name + ".wav"), stereo, 2, kRate) ||
                !mge::writePng(spectrogram(mge::analysis::channel(stereo, 2, 0)),
                               m_outDir / (name + ".png"))) {
                MGE_LOG_ERROR("Gallery", "No se pudo escribir ", name);
                setExitCode(1);
                return;
            }
            m_written++;
        }

        void writeMono(const std::string& name, const std::vector<float>& mono) {
            if (!check(name, mono)) return;
            if (!mge::writeWav(m_outDir / (name + ".wav"), mono, 1, kRate) ||
                !mge::writePng(spectrogram(mono), m_outDir / (name + ".png"))) {
                MGE_LOG_ERROR("Gallery", "No se pudo escribir ", name);
                setExitCode(1);
                return;
            }
            m_written++;
        }

        bool check(const std::string& name, const std::vector<float>& left) {
            if (mge::analysis::peak(left) < 0.01f) {
                MGE_LOG_ERROR("Gallery", name, ": salió mudo");
                setExitCode(1);
                return false;
            }
            return true;
        }

        std::filesystem::path m_outDir;
        int m_written = 0;
    };
}

mge::Application* mge::createApplication() {
    return new AudioGallery();
}
