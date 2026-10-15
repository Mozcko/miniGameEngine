# mge_add_game(): crea el ejecutable de un juego con una sola llamada.
#
#   mge_add_game(<nombre>
#       SOURCES <archivos...>              # obligatorio
#       [LIBRARIES <libs...>]              # librerías extra del juego (ej: sandbox_world)
#       [MODULES platform render audio]    # módulos del motor (por defecto: los tres)
#       [HEADLESS]                         # sin módulos: sin ventana, GPU ni SDL
#       [SMOKE_FRAMES <n>])                # registra un smoke test en CTest que corre n frames
#
# Además: enlaza mge_main (el main() del motor), aplica los warnings del proyecto, deja el
# ejecutable en la raíz del build dir y crea el target run-<nombre> (y `run` para MGE_MAIN_GAME).
set(MGE_MAIN_GAME "sandbox" CACHE STRING "Juego que ejecuta el target 'run'")

function(mge_add_game name)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "HEADLESS" "SMOKE_FRAMES" "SOURCES;LIBRARIES;MODULES")
    if(NOT ARG_SOURCES)
        message(FATAL_ERROR "mge_add_game(${name}): falta SOURCES")
    endif()
    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "mge_add_game(${name}): argumentos desconocidos: ${ARG_UNPARSED_ARGUMENTS}")
    endif()

    add_executable(${name} ${ARG_SOURCES})
    target_link_libraries(${name} PRIVATE mge_main ${ARG_LIBRARIES})

    if(ARG_HEADLESS)
        if(ARG_MODULES)
            message(FATAL_ERROR "mge_add_game(${name}): HEADLESS no admite MODULES")
        endif()
    else()
        if(NOT ARG_MODULES)
            set(ARG_MODULES platform render audio)
        endif()
        foreach(module IN LISTS ARG_MODULES)
            if(NOT module MATCHES "^(platform|render|audio)$")
                message(FATAL_ERROR "mge_add_game(${name}): módulo desconocido '${module}' (platform, render, audio)")
            endif()
            target_link_libraries(${name} PRIVATE mge_${module})
        endforeach()
    endif()

    set_target_properties(${name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
    mge_set_warnings(${name})

    # Compilar y ejecutar en un paso
    add_custom_target(run-${name} COMMAND ${name} DEPENDS ${name} USES_TERMINAL COMMENT "Ejecutando ${name}")
    if(name STREQUAL MGE_MAIN_GAME AND NOT TARGET run)
        add_custom_target(run COMMAND ${name} DEPENDS ${name} USES_TERMINAL COMMENT "Ejecutando ${name}")
    endif()

    # Smoke test: corre n frames desde otro directorio (prueba que no depende del cwd)
    if(ARG_SMOKE_FRAMES AND MGE_BUILD_TESTS)
        set(environment "MGE_MAX_FRAMES=${ARG_SMOKE_FRAMES}")
        if(ARG_HEADLESS)
            set(test_name "smoke: ${name} headless ${ARG_SMOKE_FRAMES} frames")
            set(labels "")
        else()
            set(test_name "smoke: ${name} offscreen ${ARG_SMOKE_FRAMES} frames")
            # Ventana sin pantalla (SDL offscreen + EGL). Con sanitizers se apaga solo LeakSanitizer:
            # el driver EGL de Mesa en Ubuntu pierde memoria que LSan no puede atribuir (ver #18).
            list(APPEND environment "SDL_VIDEODRIVER=offscreen" "SDL_AUDIODRIVER=dummy"
                 "ASAN_OPTIONS=detect_leaks=0:abort_on_error=1")
            set(labels gpu)
        endif()
        add_test(NAME "${test_name}" COMMAND ${name} WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/tests)
        set_tests_properties("${test_name}" PROPERTIES TIMEOUT 120 ENVIRONMENT "${environment}")
        if(labels)
            set_tests_properties("${test_name}" PROPERTIES LABELS "${labels}")
        endif()
    endif()
endfunction()
