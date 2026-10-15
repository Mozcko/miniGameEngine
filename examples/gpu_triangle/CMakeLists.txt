# Primer programa sobre SDL3 GPU (#28, #29): un triángulo que gira sobre un fondo que late.
# Los shaders (GLSL) se compilan a SPIR-V al compilar y quedan embebidos en el ejecutable.
mge_add_game(gpu_triangle
    SOURCES GpuTriangle.cpp
    MODULES platform gpu
    SMOKE_FRAMES 60)
mge_add_shaders(gpu_triangle SHADERS triangle.vert triangle.frag)
