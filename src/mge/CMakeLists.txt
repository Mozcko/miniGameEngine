# Módulos del motor. Ver docs/adr/0003-headless-core.md
#   mge_core      -> sin SDL ni GPU (siempre se compila): Application (runner + módulos), Log, Time
#   mge_procgen   -> generación procedural en CPU (headless): MeshData, primitivas, ruido, SDF
#   mge_voxel     -> mundos voxel (headless): bloques, chunks, VoxelWorld, mallado
#   mge_synth     -> audio procedural (headless): mezclador, síntesis, efectos, música
#   mge_main      -> el main() del motor (llama a createApplication del juego); solo core
#   mge_platform  -> PlatformModule: ventana e input (SDL3)
#   mge_gpu       -> GpuModule + wrappers RAII sobre SDL3 GPU (Vulkan/Metal/D3D12), ADR 0001
#   mge_graphics  -> renderer sobre mge_gpu: Texture2D, MeshRenderer (shaders embebidos)
#   mge_audio     -> AudioModule: el mezclador de mge_synth en el dispositivo de audio (SDL3)
add_subdirectory(core)
add_subdirectory(app)
# Módulos headless (sin SDL ni GPU): se compilan siempre
add_subdirectory(procgen)
add_subdirectory(voxel)
add_subdirectory(synth)

if(MGE_BUILD_MODULES)
    add_subdirectory(platform)
    add_subdirectory(gpu)
    add_subdirectory(graphics)
    add_subdirectory(audio)
endif()
