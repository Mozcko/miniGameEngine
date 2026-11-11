# Módulos del motor. Ver docs/adr/0003-headless-core.md
#   mge_core      -> sin SDL ni GPU (siempre se compila): Application (runner + módulos), Log, Time
#   mge_procgen   -> generación procedural en CPU (headless): MeshData, primitivas, ruido, SDF
#   mge_voxel     -> mundos voxel (headless): bloques, chunks, VoxelWorld, mallado
#   mge_synth     -> audio procedural (headless): mezclador, síntesis, efectos, música
#   mge_physics   -> física (headless): colisiones barridas, consultas espaciales
#   mge_main      -> el main() del motor (llama a createApplication del juego); solo core
#   mge_platform  -> PlatformModule: ventana e input (SDL3)
#   mge_gpu       -> GpuModule + wrappers RAII sobre SDL3 GPU (Vulkan/Metal/D3D12), ADR 0001
#   mge_graphics  -> renderer sobre mge_gpu: Texture2D, MeshRenderer (shaders embebidos)
#   mge_audio     -> AudioModule: el mezclador de mge_synth en el dispositivo de audio (SDL3)

# Helper para instalar headers públicos preservando la estructura relativa
function(mge_install_headers dest_subdir)
    if(NOT MGE_INSTALL)
        return()
    endif()
    foreach(hdr IN LISTS ARGN)
        get_filename_component(hdr_dir "${hdr}" DIRECTORY)
        if(hdr_dir)
            install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/${hdr}"
                    DESTINATION "include/mge/${dest_subdir}/${hdr_dir}")
        else()
            install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/${hdr}"
                    DESTINATION "include/mge/${dest_subdir}")
        endif()
    endforeach()
endfunction()

add_subdirectory(core)
add_subdirectory(app)
# Módulos headless (sin SDL ni GPU): se compilan siempre
add_subdirectory(procgen)
add_subdirectory(voxel)
add_subdirectory(synth)
add_subdirectory(physics)

if(MGE_BUILD_MODULES)
    add_subdirectory(platform)
    add_subdirectory(gpu)
    add_subdirectory(graphics)
    add_subdirectory(audio)
    add_subdirectory(c)
endif()

# Instalación y exportación del paquete mge
if(MGE_INSTALL)
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/mge.h" DESTINATION include/mge)
    install(FILES "${miniGameEngine_BINARY_DIR}/generated/include/mge/Version.h" DESTINATION include/mge)

    if(MGE_BUILD_MODULES)
        install(FILES
            "${CMAKE_CURRENT_SOURCE_DIR}/graphics/shaders/include/mge/lighting.glsl"
            "${CMAKE_CURRENT_SOURCE_DIR}/graphics/shaders/include/mge/raymarch.glsl"
            "${CMAKE_CURRENT_SOURCE_DIR}/graphics/shaders/include/mge/sdf.glsl"
            "${CMAKE_CURRENT_SOURCE_DIR}/graphics/shaders/include/mge/sdf_pass.glsl"
            DESTINATION include/mge/shaders/include/mge
        )
    endif()

    set(MGE_EXPORT_TARGETS
        mge_core
        mge_main
        mge_procgen
        mge_voxel
        mge_synth
        mge_physics
    )
    if(MGE_BUILD_MODULES)
        list(APPEND MGE_EXPORT_TARGETS
            mge_platform
            mge_gpu
            mge_graphics
            mge_audio
            mge_c
        )
    endif()

    install(TARGETS ${MGE_EXPORT_TARGETS}
        EXPORT mgeTargets
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin
        OBJECTS DESTINATION lib
        INCLUDES DESTINATION include
    )

    install(EXPORT mgeTargets
        NAMESPACE mge::
        DESTINATION lib/cmake/mge
    )
endif()
