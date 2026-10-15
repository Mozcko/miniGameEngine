# Audio (SDL_mixer por ahora; pasa a SDL3 en #9 y a síntesis en M5)
add_library(mge_audio STATIC
    AudioManager.h AudioManager.cpp
)
target_link_libraries(mge_audio PUBLIC mge_core ${SDL2_LIBRARIES} SDL2_mixer)
target_include_directories(mge_audio SYSTEM PUBLIC ${SDL2_INCLUDE_DIRS})
mge_set_warnings(mge_audio)
