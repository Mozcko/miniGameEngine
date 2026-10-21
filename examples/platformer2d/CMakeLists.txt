# Plataformero 2D mínimo (#45): prueba el camino 2D (Renderer2D, input, audio) con los niveles ASCII de
# assets/levels y los WAV de assets/sounds. La lógica (niveles + física) es headless y tiene tests.
add_library(platformer2d_logic STATIC Level.h Level.cpp Platformer.h Platformer.cpp)
target_link_libraries(platformer2d_logic PUBLIC mge_core)
target_include_directories(platformer2d_logic PUBLIC ${PROJECT_SOURCE_DIR})
mge_set_warnings(platformer2d_logic)

# Dibujo con Renderer2D (lo usa también un test de render)
add_library(platformer2d_view STATIC PlatformerView.h PlatformerView.cpp)
target_link_libraries(platformer2d_view PUBLIC platformer2d_logic mge_graphics)
mge_set_warnings(platformer2d_view)

mge_add_game(platformer2d
    SOURCES Platformer2D.cpp
    LIBRARIES platformer2d_view
    SMOKE_FRAMES 120)
