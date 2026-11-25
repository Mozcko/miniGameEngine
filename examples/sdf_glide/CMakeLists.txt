# SDF Glide demo (#66): endless SDF game where you steer an orb through a procedural canyon.
# Math and game logic (SdfGlideLogic) are headless and tested in tests/examples/test_sdf_glide.cpp.
add_library(sdf_glide_logic STATIC SdfGlideLogic.h SdfGlideLogic.cpp)
target_link_libraries(sdf_glide_logic PUBLIC mge_core mge_synth)
target_include_directories(sdf_glide_logic PUBLIC ${PROJECT_SOURCE_DIR})
mge_set_warnings(sdf_glide_logic)

mge_add_game(sdf_glide
    SOURCES SdfGlideApp.cpp
    LIBRARIES sdf_glide_logic
    SMOKE_FRAMES 60)
mge_add_shaders(sdf_glide SHADERS glide.frag)
