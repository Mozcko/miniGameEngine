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

TEST_CASE("PlatformModule: atajos de acciones (isActionDown, wasActionPressed, wasActionReleased, axis)") {
    mge::PlatformModule platform;
    platform.actions().bind("Jump", mge::Key::Space);
    platform.actions().bindAxis("MoveX", mge::Key::D, mge::Key::A);

    platform.input().beginFrame();
    platform.input().setKey(mge::Key::Space, true);
    platform.input().setKey(mge::Key::D, true);

    CHECK(platform.isActionDown("Jump"));
    CHECK(platform.wasActionPressed("Jump"));
    CHECK_FALSE(platform.wasActionReleased("Jump"));
    CHECK(platform.axis("MoveX") == doctest::Approx(1.0f));

    platform.input().beginFrame();
    CHECK(platform.isActionDown("Jump"));
    CHECK_FALSE(platform.wasActionPressed("Jump"));
    CHECK_FALSE(platform.wasActionReleased("Jump"));

    platform.input().beginFrame();
    platform.input().setKey(mge::Key::Space, false);
    CHECK_FALSE(platform.isActionDown("Jump"));
    CHECK_FALSE(platform.wasActionPressed("Jump"));
    CHECK(platform.wasActionReleased("Jump"));
}
