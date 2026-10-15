# Smoke test headless: Application sin módulos + generación de chunks del sandbox (sin ventana ni GPU).
add_executable(headless_smoke HeadlessSmoke.cpp)
target_link_libraries(headless_smoke PRIVATE sandbox_world mge_main)
mge_set_warnings(headless_smoke)

if(MGE_BUILD_TESTS)
    add_test(NAME "smoke: headless 1000 frames" COMMAND headless_smoke)
endif()
