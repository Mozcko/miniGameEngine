# src/mge/

The engine. Each subfolder is one CMake target with its own `CMakeLists.txt` and `README.md`.

| Folder | Target | Depends on | Contents | Doc |
|--------|--------|------------|----------|-----|
| `core/` | `mge_core` | std only | `Time` (steady_clock, time scale) | [core/README.md](core/README.md) |
| `platform/` | `mge_platform` | core, SDL2, GLEW, OpenGL | `Window` (SDL window, GL 3.3 context, GLEW), `InputManager` | [platform/README.md](platform/README.md) |
| `render/` | `mge_render` | core, platform*, GL, GLEW, SDL2_ttf | `core/` Vertex, Texture, GLSLProgram, ResourceManager · `3d/` Mesh · `2d/` Sprite, Animation, GUILabel · `camera/` Camera, Camera2D, Camera3D, FirstPersonCamera | [render/README.md](render/README.md) |
| `audio/` | `mge_audio` | core, SDL2_mixer | `AudioManager` | [audio/README.md](audio/README.md) |
| `app/` | `mge_app`, `mge_main` | platform, audio | `Application` (game loop), `Main.cpp` (`main()`) | [app/README.md](app/README.md) |

\* temporary, until #15.

`-DMGE_BUILD_MODULES=OFF` builds only `mge_core` and its tests, with no SDL or GPU needed.

## How M1 reshapes this
- **#11** moves `Application` into `mge_core` as a module‑based runner that can run headless.
  Platform and audio then become optional modules.
- **#9** moves platform and audio to SDL3, and removes SDL_mixer, SDL_ttf and `GUILabel`.
- **#10** puts everything in one `mge` namespace with a single naming convention.

## Writing a game on the engine (current API)

```cpp
#include <mge/app/Application.h>

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

```cmake
add_executable(mygame MyGame.cpp)
target_link_libraries(mygame PRIVATE mge_app mge_render mge_main)
```

The window starts with **depth test disabled** and blending enabled (see `Window::init`). Enable
depth testing yourself for 3D.
