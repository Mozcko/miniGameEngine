# Audio (#46): plays the mge_synth mixer on the default playback device (SDL3 audio streams)
add_library(mge_audio STATIC
    AudioModule.h AudioModule.cpp
)
add_library(mge::audio ALIAS mge_audio)
set_target_properties(mge_audio PROPERTIES EXPORT_NAME audio)
target_include_directories(mge_audio PUBLIC
    $<BUILD_INTERFACE:${miniGameEngine_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(mge_audio PUBLIC mge_core mge_synth SDL3::SDL3)
mge_set_warnings(mge_audio)

# For <mge/mge.h>: targets linking this module see MGE_HAS_AUDIO
target_compile_definitions(mge_audio INTERFACE MGE_HAS_AUDIO=1)

mge_install_headers(audio
    AudioModule.h
)
