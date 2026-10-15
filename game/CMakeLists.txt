# Sandbox: mundo voxel de prueba del motor

# Lógica del mundo (bloques y chunks): librería aparte para poder testearla
add_library(sandbox_world STATIC
    Block.h Block.cpp
    Chunk.h Chunk.cpp
)
# Permite #include <game/Chunk.h>
target_include_directories(sandbox_world PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(sandbox_world PUBLIC mge_render)
mge_set_warnings(sandbox_world)

# Ejecutable del juego
add_executable(sandbox SandboxApp.cpp)
target_link_libraries(sandbox PRIVATE sandbox_world mge_platform mge_audio mge_main)
# Se genera en la raíz del build dir (hermano de assets/) para que "../assets/..." funcione
set_target_properties(sandbox PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
mge_set_warnings(sandbox)

# Smoke test del juego real: 120 frames sin pantalla (driver offscreen de SDL3 + OpenGL por EGL).
# Etiqueta "gpu": necesita un driver OpenGL (en CI, Mesa llvmpipe). Excluir con `ctest -LE gpu`.
# Con sanitizers: solo para este test se apaga LeakSanitizer (ASan/UBSan siguen activos). El driver EGL
# de Mesa en Ubuntu 24.04 pierde memoria en eglInitialize y LSan no puede atribuirla (el driver ya se
# descargó: "unknown module"). El resto de los tests mantiene detect_leaks=1.
if(MGE_BUILD_TESTS)
    add_test(NAME "smoke: sandbox offscreen 120 frames" COMMAND sandbox WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties("smoke: sandbox offscreen 120 frames" PROPERTIES
        LABELS gpu
        TIMEOUT 120
        ENVIRONMENT "SDL_VIDEODRIVER=offscreen;SDL_AUDIODRIVER=dummy;MGE_MAX_FRAMES=120;ASAN_OPTIONS=detect_leaks=0:abort_on_error=1")
endif()
