# Benchmarks (no son tests): ejecutables para medir rendimiento. Solo dependen del core.
add_executable(ecs_bench ecs_bench.cpp)
target_link_libraries(ecs_bench PRIVATE mge_core)
mge_set_warnings(ecs_bench)

add_executable(noise_bench noise_bench.cpp)
target_link_libraries(noise_bench PRIVATE mge_procgen)
mge_set_warnings(noise_bench)
