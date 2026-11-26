#!/usr/bin/env bash
# Installs SDL3 system-wide in CI (ubuntu-24.04).
# Tries the package manager first (in case libsdl3-dev is already available);
# otherwise builds release-3.4.18 from source and installs it into /usr/local.
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
