#include <mge/app/Application.h>

// Punto de entrada del Motor
// El motor toma el control del main y llama a la aplicación definida por el cliente
int main(int /*argc*/, char** /*argv*/) {
    auto app = Engine::CreateApplication();
    app->Run();
    delete app;
    return 0;
}