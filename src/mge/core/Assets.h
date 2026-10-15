#pragma once

#include <filesystem>
#include <string_view>

namespace mge {

    // Resolución de rutas de assets, independiente del directorio de trabajo.
    // La raíz se busca una sola vez (y se informa en el log), en este orden:
    //   1. variable de entorno MGE_ASSET_DIR
    //   2. <carpeta del ejecutable>/assets       (builds empaquetados)
    //   3. <carpeta del ejecutable>/../assets    (layout actual: build/sandbox -> assets/)
    //   4. assets/ del código fuente (ruta absoluta fijada al configurar CMake)
    // Si ninguna existe se usa "assets" relativo (con un aviso).
    // Nota: los assets en archivos desaparecen en M8 (ADR 0002); mientras tanto esto evita depender del cwd.

    // Ruta completa de un asset: assetPath("shaders/basic3d.vert")
    std::filesystem::path assetPath(std::string_view relative);

    // Raíz de assets en uso (la resuelve en la primera llamada)
    const std::filesystem::path& assetRoot();

    // Fija la raíz a mano (tiene prioridad sobre la búsqueda automática). Vacío = volver a buscar.
    void setAssetRoot(const std::filesystem::path& root);

    // Carpeta del ejecutable actual (vacía si no se pudo determinar)
    std::filesystem::path executableDir();
}
