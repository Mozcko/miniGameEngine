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

# El mismo smoke test con los WAV (los dos caminos de sonido siguen funcionando)
if(MGE_BUILD_TESTS)
    add_test(NAME "smoke: platformer2d offscreen 120 frames (WAV sounds)" COMMAND platformer2d
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/tests)
    set_tests_properties("smoke: platformer2d offscreen 120 frames (WAV sounds)" PROPERTIES TIMEOUT 120 LABELS gpu
        FAIL_REGULAR_EXPRESSION "Validation Error|ERROR"
        ENVIRONMENT "MGE_MAX_FRAMES=120;PLATFORMER_SOUNDS=wav;SDL_VIDEODRIVER=offscreen;SDL_AUDIODRIVER=dummy;ASAN_OPTIONS=detect_leaks=0:abort_on_error=1")
endif()
