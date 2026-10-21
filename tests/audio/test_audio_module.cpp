// AudioModule (#46) con el driver "dummy" de SDL: no hace falta placa de sonido
#include <doctest/doctest.h>
#include <mge/audio/AudioModule.h>
#include <mge/core/Application.h>
#include <mge/core/Log.h>

#include <SDL3/SDL.h>

#include <chrono>
#include <thread>

using namespace mge;

TEST_CASE("AudioModule: abre el dispositivo dummy, carga WAVs del mezclador y cierra") {
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
    Application app(ApplicationConfig{});
    AudioModule audio;
    REQUIRE(audio.onInit(app));
    CHECK(audio.hasDevice());

    const SampleId jump = audio.loadWav("sounds/jump.wav");
    REQUIRE(jump != kInvalidSample);
    const Sample* sample = audio.mixer().sample(jump);
    REQUIRE(sample != nullptr);
    CHECK(sample->channels == 1);
    // jump.wav dura ~0.09 s; convertido a 48 kHz
    CHECK(sample->frameCount() > 2400);
    CHECK(sample->frameCount() < 48000);
    {
        Log::setSink([](const LogRecord&) {}); // El error esperado no ensucia la salida
        CHECK(audio.loadWav("sounds/no_existe.wav") == kInvalidSample);
        Log::setSink(nullptr);
    }

    CHECK(audio.play(jump));
    CHECK(audio.playTone({440.0f, 0.05f}));
    // El hilo de audio de SDL consume la cola
    for (int i = 0; i < 200 && audio.stats().activeVoices == 0; i++)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    audio.setMuted(true);
    CHECK(audio.muted());
    audio.onShutdown(app);
    CHECK_FALSE(audio.hasDevice());
    SDL_ResetHint(SDL_HINT_AUDIO_DRIVER);
}
