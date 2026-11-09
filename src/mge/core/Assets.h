#pragma once

#include <filesystem>
#include <string_view>

namespace mge {

    // Resolución de rutas de assets, independiente del directorio de trabajo.
    // La raíz se busca una sola vez de forma perezosa (en la primera llamada a assetPath() o assetRoot()):
    //   1. variable de entorno MGE_ASSET_DIR
    //   2. <carpeta del ejecutable>/assets       (builds empaquetados)
    //   3. <carpeta del ejecutable>/../assets    (layout actual: build/sandbox -> assets/)
    //   4. assets/ del código fuente (ruta absoluta fijada al configurar CMake, si
    //   MGE_SOURCE_ASSET_FALLBACK=ON)
    // Si ninguna existe se usa "assets" relativo (sin avisos; el fallo se reporta al intentar abrir un
    // archivo). ADR 0004: la carga de assets es un camino soportado y opcional; el motor es procedural-first.

    // Ruta completa de un asset: assetPath("textures/atlas.png")
    std::filesystem::path assetPath(std::string_view relative);

    // Raíz de assets en uso (la resuelve en la primera llamada)
    const std::filesystem::path& assetRoot();

    // Fija la raíz a mano (tiene prioridad sobre la búsqueda automática). Vacío = volver a buscar.
    void setAssetRoot(const std::filesystem::path& root);

    // Carpeta del ejecutable actual (vacía si no se pudo determinar)
    std::filesystem::path executableDir();
}
