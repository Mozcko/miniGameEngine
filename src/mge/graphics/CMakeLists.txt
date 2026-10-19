# Renderer de alto nivel sobre mge_gpu (SDL3 GPU): texturas, dibujo de mallas con cámara y profundidad.
# Los shaders son GLSL compilados a SPIR-V en el build y embebidos (mge_add_shaders).
add_library(mge_graphics STATIC
    Texture2D.h Texture2D.cpp
    MeshRenderer.h MeshRenderer.cpp
    RenderQueue.h RenderQueue.cpp
    Lighting.h
    DebugDraw.h DebugDraw.cpp
)
target_link_libraries(mge_graphics PUBLIC mge_gpu mge_procgen)
mge_set_warnings(mge_graphics)
mge_add_shaders(mge_graphics SHADERS shaders/mesh.vert shaders/mesh.frag shaders/mesh_cutout.frag
    shaders/debug_lines.vert shaders/debug_lines.frag)

# Para <mge/mge.h>: quien enlace este módulo ve MGE_HAS_GRAPHICS
target_compile_definitions(mge_graphics INTERFACE MGE_HAS_GRAPHICS=1)
