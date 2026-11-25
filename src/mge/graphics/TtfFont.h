#pragma once

#include <mge/procgen/Font.h>

#include <cstdint>
#include <filesystem>
#include <vector>

namespace mge {

    // Optional TTF fonts (#41), baked with stb_truetype into an atlas (no SDL_ttf). The default font
    // remains Font::builtin(), which requires no files.
    // pixelHeight: line height in pixels. On failure, an error is logged and the returned font is invalid.
    Font loadTtfFont(const std::vector<std::uint8_t>& ttfBytes, float pixelHeight);
    // relative path -> resolved via mge::assetPath (assets/)
    Font loadTtfFontFile(const std::filesystem::path& path, float pixelHeight);
}
