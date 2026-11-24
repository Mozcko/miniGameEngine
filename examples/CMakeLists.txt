# Examples: one subdirectory per program.

# Headless: always built (even with MGE_BUILD_MODULES=OFF)
add_subdirectory(procgen_gallery)
add_subdirectory(audio_gallery)

# The rest require the SDL/GPU modules
if(NOT MGE_BUILD_MODULES)
    return()
endif()
add_subdirectory(headless_smoke)
add_subdirectory(gpu_triangle)
add_subdirectory(platformer2d)
add_subdirectory(tone)
add_subdirectory(sdf_scene)
add_subdirectory(sdf_glide)
add_subdirectory(guide_game)
add_subdirectory(c_hello)
# scripts/new-game.sh appends lines below
