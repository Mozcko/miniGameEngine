# main() del motor: llama a mge::createApplication() del juego y devuelve el código de salida de run().
# OBJECT para que el main() se enlace siempre en el ejecutable. Solo depende del core (sin SDL),
# así que sirve también para programas headless.
add_library(mge_main OBJECT Main.cpp)
add_library(mge::main ALIAS mge_main)
set_target_properties(mge_main PROPERTIES EXPORT_NAME main)
target_include_directories(mge_main PUBLIC
    $<BUILD_INTERFACE:${miniGameEngine_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(mge_main PUBLIC mge_core)
mge_set_warnings(mge_main)
