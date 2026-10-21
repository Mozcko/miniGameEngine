// Sonido sin archivos (#46): un arpegio de tonos senoidales generados por el mezclador del motor. Sin
// ventana ni GPU: solo el AudioModule. Suena ~2 segundos y termina (con SDL_AUDIODRIVER=dummy, en silencio).
#include <mge/mge.h>

#include <array>
#include <chrono>
#include <thread>

namespace {
    class ToneApp : public mge::Application {
    public:
        ToneApp() : mge::Application(mge::ApplicationConfig{}) { addModule<mge::AudioModule>(); }

    protected:
        bool onInit() override {
            m_audio = getModule<mge::AudioModule>();
            MGE_LOG_INFO("Tone", "Arpegio de Do mayor generado por código (", m_audio->mixer().sampleRate(),
                         " Hz)");
            return true;
        }

        void onUpdate(float /*dt*/) override {
            // Do, Mi, Sol, Do agudo: una nota cada 0.25 s, de izquierda a derecha, y el acorde al final
            constexpr std::array<float, 4> kNotes = {261.63f, 329.63f, 392.0f, 523.25f};
            m_elapsed += time().unscaledDelta();
            while (m_next < kNotes.size() && m_elapsed >= 0.25f * float(m_next)) {
                const float pan = -0.6f + 0.4f * float(m_next);
                m_audio->playTone({kNotes[m_next], 0.3f, 0.35f, pan});
                m_next++;
            }
            if (m_next == kNotes.size() && !m_chord && m_elapsed >= 1.1f) {
                for (float note : kNotes) m_audio->playTone({note, 0.8f, 0.15f, 0.0f});
                m_chord = true;
            }
            if (m_elapsed >= 2.0f) requestStop();
            // Sin ventana el loop no espera al vsync: no gastar un núcleo entero
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }

    private:
        mge::AudioModule* m_audio = nullptr;
        float m_elapsed = 0.0f;
        std::size_t m_next = 0;
        bool m_chord = false;
    };
}

mge::Application* mge::createApplication() {
    return new ToneApp();
}
