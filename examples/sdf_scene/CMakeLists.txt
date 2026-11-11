# Escena de demostración de SDF raymarching (#36): formas SDF (blobs con unión suave,
# toro con torsión, campo de columnas) intersectando una malla de plano de suelo y un cubo rasterizados.
mge_add_game(sdf_scene
    SOURCES SdfScene.cpp
    MODULES platform gpu graphics
    SMOKE_FRAMES 60)
mge_add_shaders(sdf_scene SHADERS scene.frag)
