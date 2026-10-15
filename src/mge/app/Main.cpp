#include <mge/app/Application.h>
// SDL3: en las plataformas que lo necesitan, SDL_main.h provee el punto de entrada real
#include <SDL3/SDL_main.h>

// Punto de entrada del Motor
// El motor toma el control del main y llama a la aplicación definida por el cliente
int main(int /*argc*/, char** /*argv*/) {
    auto app = mge::createApplication();
    app->run();
    delete app;
    return 0;
}