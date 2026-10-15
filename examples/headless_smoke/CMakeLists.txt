# Smoke test headless: Application sin módulos + generación de chunks del sandbox (sin ventana ni GPU).
# El programa fija sus propios 1000 frames (config.maxFrames).
mge_add_game(headless_smoke
    HEADLESS
    SOURCES HeadlessSmoke.cpp
    LIBRARIES sandbox_world
    SMOKE_FRAMES 1000)
