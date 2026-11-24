# Headless smoke test: Application without modules + sandbox chunk generation (no window or GPU).
# The program sets its own 1000 frames (config.maxFrames).
mge_add_game(headless_smoke
    HEADLESS
    SOURCES HeadlessSmoke.cpp
    LIBRARIES sandbox_world
    SMOKE_FRAMES 1000)
