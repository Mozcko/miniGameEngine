#pragma once

#include <mge/core/Module.h>

namespace mge {

    // Módulo de audio: inicializa y cierra el AudioManager. Si no hay dispositivo de audio
    // el juego sigue en silencio (no es un error fatal).
    class AudioModule : public Module {
    public:
        const char* name() const override { return "Audio"; }
        bool onInit(Application& app) override;
        void onShutdown(Application& app) override;
    };
}
