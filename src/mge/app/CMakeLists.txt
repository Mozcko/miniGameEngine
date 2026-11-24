# Engine main(): calls the game's mge::createApplication() and returns the exit code from run().
# OBJECT library so main() is always linked into the executable. Depends only on core (no SDL),
# so it also works for headless programs.
add_library(mge_main OBJECT Main.cpp)
add_library(mge::main ALIAS mge_main)
set_target_properties(mge_main PROPERTIES EXPORT_NAME main)
target_include_directories(mge_main PUBLIC
    $<BUILD_INTERFACE:${miniGameEngine_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(mge_main PUBLIC mge_core)
mge_set_warnings(mge_main)
