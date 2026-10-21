# Ejemplos: un subdirectorio por programa.

# Headless: se compilan siempre (también con MGE_BUILD_MODULES=OFF)
add_subdirectory(procgen_gallery)

# El resto necesita los módulos con SDL/GPU
if(NOT MGE_BUILD_MODULES)
    return()
endif()
add_subdirectory(headless_smoke)
add_subdirectory(gpu_triangle)
add_subdirectory(platformer2d)
add_subdirectory(tone)
# scripts/new-game.sh agrega líneas debajo
