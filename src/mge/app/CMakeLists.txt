# main() del motor: llama a mge::createApplication() del juego y devuelve el código de salida de run().
# OBJECT para que el main() se enlace siempre en el ejecutable. Solo depende del core (sin SDL),
# así que sirve también para programas headless.
add_library(mge_main OBJECT Main.cpp)
target_link_libraries(mge_main PUBLIC mge_core)
mge_set_warnings(mge_main)
