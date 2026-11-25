# Engine modules. See docs/adr/0003-headless-core.md
#   mge_core      -> no SDL or GPU (always built): Application (runner + modules), Log, Time
#   mge_procgen   -> CPU procedural generation (headless): MeshData, primitives, noise, SDF
#   mge_voxel     -> voxel worlds (headless): blocks, chunks, VoxelWorld, meshing
#   mge_synth     -> procedural audio (headless): mixer, synthesis, effects, music
#   mge_physics   -> physics (headless): swept collisions, spatial queries
#   mge_main      -> the engine's main() (calls the game's createApplication); core only
#   mge_platform  -> PlatformModule: window and input (SDL3)
#   mge_gpu       -> GpuModule + RAII wrappers over SDL3 GPU (Vulkan/Metal/D3D12), ADR 0001
#   mge_graphics  -> renderer over mge_gpu: Texture2D, MeshRenderer (embedded shaders)
#   mge_audio     -> AudioModule: the mge_synth mixer on the audio device (SDL3)

# Helper to install public headers preserving relative directory structure
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
# Headless modules (no SDL or GPU): always built
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

# Installation and export of the mge package
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
