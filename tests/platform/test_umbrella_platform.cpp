// <mge/mge.h> linking mge_platform: must include the platform module
#include <doctest/doctest.h>
#include <mge/mge.h>

#ifndef MGE_HAS_PLATFORM
#error "mge_platform_tests links mge_platform: MGE_HAS_PLATFORM should be defined"
#endif

TEST_CASE("<mge/mge.h> includes PlatformModule when linking mge_platform") {
    mge::PlatformModule platform; // Without onInit: does not open a window
    CHECK(std::string(platform.name()) == "Platform");
    CHECK_FALSE(platform.isCursorLocked());
}

TEST_CASE("PlatformModule: action shortcuts (isActionDown, wasActionPressed, wasActionReleased, axis)") {
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
