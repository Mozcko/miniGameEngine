# src/mge/app/  →  target `mge_main`

The engine's `main()`. It is an **OBJECT library** linked into each game executable, and it depends
only on `mge_core`, so headless programs use it too.

```cpp
int main(int, char**) {
    std::unique_ptr<mge::Application> app(mge::createApplication());   // defined by the game
    if (!app) return 1;
    return app->run();                                                  // 0 = ok, 1 = init failed
}
```

`Application` itself lives in the core (`src/mge/core/Application.h`) since #11. See
[core/README.md](../core/README.md).

`SDL_main.h` isn't needed with SDL3 on Linux/Windows. If a platform ever requires it, the game can
include it in its own source file, keeping `mge_main` free of SDL.
