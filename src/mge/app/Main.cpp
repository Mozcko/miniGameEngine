#include <mge/core/Application.h>

#include <memory>

// Engine entry point: creates the game-defined application and runs it.
// Does not depend on SDL, so it also works for headless programs.
// (With SDL3 on Linux/Windows SDL_main.h is not needed; if ever ported to a
// platform that requires it, the game can include it in its own file.)
int main(int /*argc*/, char** /*argv*/) {
    std::unique_ptr<mge::Application> app(mge::createApplication());
    if (!app) return 1;
    return app->run();
}
