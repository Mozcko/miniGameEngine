add_library(mge_c STATIC
    App.cpp
    Audio.cpp
    Ecs.cpp
    Errors.cpp
    Input.cpp
    Render.cpp
    VoxelPhysics.cpp
    mge_c.h
    mge_c_internal.h
)
add_library(mge::c ALIAS mge_c)
set_target_properties(mge_c PROPERTIES EXPORT_NAME c)
target_include_directories(mge_c PUBLIC
    $<BUILD_INTERFACE:${miniGameEngine_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(mge_c PUBLIC
    mge_core
    mge_procgen
    mge_voxel
    mge_synth
    mge_physics
)

if(UNIX AND NOT APPLE)
    target_link_libraries(mge_c PUBLIC m)
endif()

if(MGE_BUILD_MODULES)
    target_link_libraries(mge_c PUBLIC
        mge_platform
        mge_gpu
        mge_graphics
        mge_audio
    )
endif()

mge_set_warnings(mge_c)

if(MGE_BUILD_C_SHARED)
    add_library(mge_c_shared SHARED
        App.cpp
        Audio.cpp
        Ecs.cpp
        Errors.cpp
        Input.cpp
        Render.cpp
        VoxelPhysics.cpp
        mge_c.h
        mge_c_internal.h
    )
    target_compile_definitions(mge_c_shared PRIVATE MGE_C_BUILD_SHARED=1)
    target_include_directories(mge_c_shared PUBLIC
        $<BUILD_INTERFACE:${miniGameEngine_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:include>
    )
    target_link_libraries(mge_c_shared PUBLIC
        mge_core
        mge_procgen
        mge_voxel
        mge_synth
        mge_physics
    )
    if(MGE_BUILD_MODULES)
        target_link_libraries(mge_c_shared PUBLIC
            mge_platform
            mge_gpu
            mge_graphics
            mge_audio
        )
    endif()
    set_target_properties(mge_c_shared PROPERTIES
        OUTPUT_NAME mge_c
        C_VISIBILITY_PRESET hidden
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
    )
    mge_set_warnings(mge_c_shared)
endif()

mge_install_headers(c mge_c.h)
