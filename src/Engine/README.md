# src/Engine/

The reusable engine layer. It owns `main()`, the window, the game loop and all
platform/graphics wrappers. A game plugs in through `Engine::Application`.

```
Engine/
├── Main.cpp      main(): CreateApplication() → Run() → delete
├── Core/         Application (game loop), Time (scaled delta time), ResourceManager (texture cache)
├── Window/       SDL2 window + OpenGL 3.3 core context + GLEW init
├── Input/        InputManager: keyboard state, mouse position/delta, cursor lock
├── Audio/        AudioManager: SDL_mixer SFX + music with path caches
└── Graphics/     Everything that touches OpenGL
    ├── Core/     Vertex struct, Texture, GLSLProgram (shaders)
    ├── 3D/       Mesh (VAO/VBO/EBO)
    ├── 2D/       Sprite (textured quad), Animation (frame sequencer), GUILabel (TTF text)
    └── Camera/   Camera (abstract 2D‑style), Camera2D, Camera3D (fly), FirstPersonCamera (walk)
```

## `Main.cpp`

```cpp
int main(int argc, char** argv) {
    auto app = Engine::CreateApplication();   // provided by the game (src/Game/SandboxApp.cpp)
    app->Run();
    delete app;
    return 0;
}
```
`argc/argv` are ignored. There is no other `main` in the project.

## Subsystem docs

| Folder | Doc |
|--------|-----|
| Core | [Core/README.md](Core/README.md) |
| Window | [Window/README.md](Window/README.md) |
| Input | [Input/README.md](Input/README.md) |
| Audio | [Audio/README.md](Audio/README.md) |
| Graphics | [Graphics/README.md](Graphics/README.md) |

## How to write a new game on this engine

```cpp
#include <Engine/Core/Application.h>

class MyGame : public Engine::Application {
public:
    void OnInit() override { /* load shaders/textures, set GL state */ }
    void OnEvent(SDL_Event& e) override {
        if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) m_Running = false;
    }
    void OnUpdate(float dt) override { /* logic */ }
    void OnRender() override { /* glClear + draw; buffers are swapped by the engine */ }
};

Engine::Application* Engine::CreateApplication() { return new MyGame(); }
```

Remove (or exclude from the glob) `src/Game/SandboxApp.cpp` – only one
`CreateApplication` definition may exist. Remember the window starts with **depth test
disabled** and blending enabled (`Window::init`); enable depth testing yourself for 3D.
