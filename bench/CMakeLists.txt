# Benchmarks (no son tests): ejecutables para medir rendimiento. Solo dependen del core.
add_executable(mge_bench mge_bench.cpp)
target_include_directories(mge_bench PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(mge_bench PRIVATE mge_core mge_procgen mge_voxel mge_physics)
mge_set_warnings(mge_bench)

add_executable(ecs_bench ecs_bench.cpp)
target_link_libraries(ecs_bench PRIVATE mge_core)
mge_set_warnings(ecs_bench)

add_executable(noise_bench noise_bench.cpp)
target_link_libraries(noise_bench PRIVATE mge_procgen)
mge_set_warnings(noise_bench)
