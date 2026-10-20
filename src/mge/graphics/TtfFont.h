#pragma once

#include <mge/procgen/Font.h>

#include <cstdint>
#include <filesystem>
#include <vector>

namespace mge {

    // Fuentes TTF opcionales (#41), horneadas con stb_truetype en un atlas (sin SDL_ttf). La fuente por
    // defecto sigue siendo Font::builtin(), que no necesita archivos.
    // pixelHeight: alto de la línea en píxeles. Si algo falla se registra en el log y la fuente es inválida.
    Font loadTtfFont(const std::vector<std::uint8_t>& ttfBytes, float pixelHeight);
    // path relativo → se busca con mge::assetPath (assets/)
    Font loadTtfFontFile(const std::filesystem::path& path, float pixelHeight);
}
