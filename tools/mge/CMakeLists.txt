# Herramienta de línea de comandos del motor (mge CLI)

add_library(mge_cli_core STATIC
    Process.h Process.cpp
    Colors.h
    Project.h Project.cpp
    Doctor.h Doctor.cpp
    Commands.h Commands.cpp
)

target_include_directories(mge_cli_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${miniGameEngine_SOURCE_DIR}/src
    ${miniGameEngine_BINARY_DIR}/generated/include
)

target_compile_definitions(mge_cli_core PUBLIC
    MGE_SOURCE_TEMPLATES_DIR="${miniGameEngine_SOURCE_DIR}/templates"
)

target_link_libraries(mge_cli_core PUBLIC mge_core)
mge_set_warnings(mge_cli_core)

add_executable(mge Main.cpp)
target_link_libraries(mge PRIVATE mge_cli_core)
set_target_properties(mge PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
mge_set_warnings(mge)

if(MGE_INSTALL)
    install(TARGETS mge RUNTIME DESTINATION bin)
endif()
