# assets/fonts/

TrueType pixel fonts rendered through SDL2_ttf by `mge::GUILabel`.

| File | Used for |
|------|----------|
| `PixelOperator8-Bold.ttf` | FPS label (`SandboxApp::m_fpsLabel`, size 24, yellow) |
| `PixelOperator8.ttf` | Info label (`SandboxApp::m_infoLabel`, size 16, white) |

"Pixel Operator" is a free pixel font family (believed to be by Jayvee Enaguas under CC0 – no
licence file is included in the repo; verify before redistributing). The 8‑px design scales
cleanly at multiples of 8.

Load with `GUILabel::init("../assets/fonts/<file>.ttf", pointSize)`; requires `TTF_Init()`,
which `mge::Application` does in its constructor.
