# Primer programa sobre SDL3 GPU (#28): una ventana que limpia la imagen con un color que late.
mge_add_game(gpu_clear
    SOURCES GpuClear.cpp
    MODULES platform gpu
    SMOKE_FRAMES 60)
