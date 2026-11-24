# First program on SDL3 GPU (#28, #29): a spinning triangle over a pulsing background.
# Shaders (GLSL) are compiled to SPIR-V at build time and embedded in the executable.
mge_add_game(gpu_triangle
    SOURCES GpuTriangle.cpp
    MODULES platform gpu
    SMOKE_FRAMES 60)
mge_add_shaders(gpu_triangle SHADERS triangle.vert triangle.frag)
