// <mge/mge.h> enlazando mge_platform: debe incluir el módulo de plataforma
#include <doctest/doctest.h>
#include <mge/mge.h>

#ifndef MGE_HAS_PLATFORM
#error "mge_platform_tests enlaza mge_platform: MGE_HAS_PLATFORM debería estar definido"
#endif

TEST_CASE("<mge/mge.h> incluye PlatformModule cuando se enlaza mge_platform") {
    mge::PlatformModule platform; // Sin onInit: no abre ventana
    CHECK(std::string(platform.name()) == "Platform");
    CHECK_FALSE(platform.isCursorLocked());
}
