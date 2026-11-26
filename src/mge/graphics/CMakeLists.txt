# High-level renderer on top of mge_gpu (SDL3 GPU): textures, mesh drawing with camera and depth.
# Shaders are GLSL compiled to SPIR-V during the build and embedded (mge_add_shaders).
add_library(mge_graphics STATIC
    Texture2D.h Texture2D.cpp
    MeshRenderer.h MeshRenderer.cpp
    RenderQueue.h RenderQueue.cpp
    Lighting.h
    DebugDraw.h DebugDraw.cpp
    TtfFont.h TtfFont.cpp
    Batch2D.h Batch2D.cpp
    Renderer2D.h Renderer2D.cpp
    DebugOverlay.h DebugOverlay.cpp
    DayNight.h DayNight.cpp
    Sky.h Sky.cpp
    SdfRenderer.h SdfRenderer.cpp
    GpuModel.h GpuModel.cpp
)
add_library(mge::graphics ALIAS mge_graphics)
set_target_properties(mge_graphics PROPERTIES EXPORT_NAME graphics)
target_include_directories(mge_graphics PUBLIC
    $<BUILD_INTERFACE:${miniGameEngine_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(mge_graphics PUBLIC mge_gpu mge_procgen mge_gltf)
mge_set_warnings(mge_graphics)
mge_add_shaders(mge_graphics SHADERS shaders/mesh.vert shaders/mesh.frag shaders/mesh_cutout.frag
    shaders/debug_lines.vert shaders/debug_lines.frag shaders/ui.vert shaders/ui.frag
    shaders/sky.vert shaders/sky.frag)

# For <mge/mge.h>: anything linking this module sees MGE_HAS_GRAPHICS
target_compile_definitions(mge_graphics INTERFACE MGE_HAS_GRAPHICS=1)

mge_install_headers(graphics
    Batch2D.h
    DayNight.h
    DebugDraw.h
    DebugOverlay.h
    GpuModel.h
    Lighting.h
    MeshRenderer.h
    Renderer2D.h
    RenderQueue.h
    SdfRenderer.h
    Sky.h
    Texture2D.h
    TtfFont.h
)
