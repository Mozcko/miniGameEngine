#pragma once

#include <filesystem>
#include <string_view>

namespace mge {

    // Asset path resolution, independent of the working directory.
    // The root is resolved lazily once (on the first call to assetPath() or assetRoot()):
    //   1. MGE_ASSET_DIR environment variable
    //   2. <executable directory>/assets       (packaged builds)
    //   3. <executable directory>/../assets    (current layout: build/sandbox -> assets/)
    //   4. source tree assets/ (absolute path fixed at CMake configure time, if
    //   MGE_SOURCE_ASSET_FALLBACK=ON)
    // If none exists, relative "assets" is used (silently; failure is reported when attempting to open a
    // file). ADR 0004: asset loading is a supported optional path; the engine is procedural-first.

    // Full path to an asset: assetPath("textures/atlas.png")
    std::filesystem::path assetPath(std::string_view relative);

    // Asset root currently in use (resolved on first call)
    const std::filesystem::path& assetRoot();

    // Manually set the root (takes priority over automatic lookup). Empty = resolve again.
    void setAssetRoot(const std::filesystem::path& root);

    // Directory of the current executable (empty if it could not be determined)
    std::filesystem::path executableDir();
}
