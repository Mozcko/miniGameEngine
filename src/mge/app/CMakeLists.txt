# Application: crea la ventana, inicializa audio/TTF y corre el loop
add_library(mge_app STATIC
    Application.h Application.cpp
)
target_link_libraries(mge_app PUBLIC mge_platform mge_audio SDL2_ttf)
mge_set_warnings(mge_app)

# main() del motor. OBJECT para que el main() se enlace siempre en el ejecutable del juego.
add_library(mge_main OBJECT Main.cpp)
target_link_libraries(mge_main PUBLIC mge_app)
mge_set_warnings(mge_main)
