#include <doctest/doctest.h>
#include <mge/Version.h>

#include <string_view>

TEST_CASE("version: macros y constantes de versión coinciden") {
    CHECK(MGE_VERSION_MAJOR == 1);
    CHECK(MGE_VERSION_MINOR == 0);
    CHECK(MGE_VERSION_PATCH == 0);

    CHECK(mge::kVersion.major == 1);
    CHECK(mge::kVersion.minor == 0);
    CHECK(mge::kVersion.patch == 0);

    CHECK(std::string_view(MGE_VERSION_STRING) == "1.0.0");
    CHECK(std::string_view(mge::kVersionString) == "1.0.0");
}
