# GPU de bajo nivel sobre la API SDL3 GPU (Vulkan/Metal/D3D12), ver ADR 0001.
# Wrappers RAII, GpuModule (device + swapchain de la ventana) y subida de MeshData.
add_library(mge_gpu STATIC
    Handles.h
    Shader.h
    Pipeline.h
    GpuDevice.h GpuDevice.cpp
    CommandBuffer.h CommandBuffer.cpp
    GpuMesh.h GpuMesh.cpp
    GpuModule.h GpuModule.cpp
    GpuNoiseField.h GpuNoiseField.cpp
    GpuResourceTracker.h
    Swapchain.h Swapchain.cpp
)
add_library(mge::gpu ALIAS mge_gpu)
set_target_properties(mge_gpu PROPERTIES EXPORT_NAME gpu)
target_include_directories(mge_gpu PUBLIC
    $<BUILD_INTERFACE:${miniGameEngine_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)
mge_add_shaders(mge_gpu SHADERS shaders/noise.comp)
target_link_libraries(mge_gpu PUBLIC mge_core mge_procgen mge_platform SDL3::SDL3)
# Validación de la GPU (capas de Vulkan) por defecto en Debug
target_compile_definitions(mge_gpu PRIVATE $<$<CONFIG:Debug>:MGE_GPU_DEBUG_DEFAULT=1>)
mge_set_warnings(mge_gpu)

# Para <mge/mge.h>: quien enlace este módulo ve MGE_HAS_GPU
target_compile_definitions(mge_gpu INTERFACE MGE_HAS_GPU=1)

mge_install_headers(gpu
    CommandBuffer.h
    GpuDevice.h
    GpuMesh.h
    GpuModule.h
    GpuNoiseField.h
    GpuResourceTracker.h
    Handles.h
    Pipeline.h
    Shader.h
    Swapchain.h
)
