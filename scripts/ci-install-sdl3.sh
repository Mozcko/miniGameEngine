#!/usr/bin/env bash
# Instala SDL3 en el sistema en CI (ubuntu-24.04).
# Intenta primero vía gestor de paquetes (por si libsdl3-dev ya está disponible);
# si no, compila release-3.4.18 desde el código fuente y lo instala en /usr/local.
set -euo pipefail

if sudo apt-get install -y --no-install-recommends libsdl3-dev 2>/dev/null; then
    echo "Installed libsdl3-dev from package manager"
else
    echo "libsdl3-dev is not available in repositories; building SDL3 release-3.4.18 from source and installing to /usr/local"
    TMP_SDL=$(mktemp -d)
    curl -sSL https://github.com/libsdl-org/SDL/archive/refs/tags/release-3.4.18.tar.gz | tar -xz -C "$TMP_SDL"
    cmake -S "$TMP_SDL/SDL-release-3.4.18" -B "$TMP_SDL/build" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DSDL_SHARED=ON \
        -DSDL_STATIC=OFF \
        -DSDL_TEST_LIBRARY=OFF \
        -DSDL_TESTS=OFF \
        -DSDL_EXAMPLES=OFF \
        -DSDL_INSTALL=ON
    sudo cmake --build "$TMP_SDL/build" --target install
    sudo ldconfig
    rm -rf "$TMP_SDL"
fi
