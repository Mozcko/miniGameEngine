#include <mge/core/Application.h>

#include <memory>

// Punto de entrada del motor: crea la aplicación definida por el juego y la corre.
// No depende de SDL, así que sirve también para programas headless.
// (Con SDL3 en Linux/Windows no hace falta SDL_main.h; si algún día se porta a una
// plataforma que lo exija, el juego puede incluirlo en su propio archivo.)
int main(int /*argc*/, char** /*argv*/) {
    std::unique_ptr<mge::Application> app(mge::createApplication());
    if (!app) return 1;
    return app->run();
}
