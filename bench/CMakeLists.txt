# Benchmarks (no son tests): ejecutables para medir rendimiento. Solo dependen del core.
add_executable(ecs_bench ecs_bench.cpp)
target_link_libraries(ecs_bench PRIVATE mge_core)
mge_set_warnings(ecs_bench)
