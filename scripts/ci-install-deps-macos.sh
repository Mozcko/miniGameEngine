#!/usr/bin/env bash
# CI dependencies for macOS (Apple Silicon, Metal via SDL3 GPU).
# Installs build tools, SDL3, GLM, glslang and spirv-cross via Homebrew, and places
# pinned stb headers (<stb/stb_image.h>, <stb/stb_image_write.h>, <stb/stb_truetype.h>)
# under $(brew --prefix)/include/stb.
set -euo pipefail

export HOMEBREW_NO_AUTO_UPDATE=1
export HOMEBREW_NO_INSTALL_CLEANUP=1

brew install cmake ninja ccache glm glslang spirv-cross sdl3

BREW_PREFIX="$(brew --prefix)"
STB_DIR="${BREW_PREFIX}/include/stb"
STB_COMMIT="f056911"
mkdir -p "${STB_DIR}"

for header in stb_image.h stb_image_write.h stb_truetype.h; do
    if [ ! -f "${STB_DIR}/${header}" ]; then
        curl -fsSL "https://raw.githubusercontent.com/nothings/stb/${STB_COMMIT}/${header}" \
            -o "${STB_DIR}/${header}"
    fi
done
