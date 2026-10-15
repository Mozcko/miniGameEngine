#!/usr/bin/env bash
# Dependencias de CI (Ubuntu 24.04). SDL3 no viene empaquetado en 24.04, así que se compila
# desde el código fuente (FetchContent): aquí van sus headers de desarrollo de Linux
# (https://wiki.libsdl.org/SDL3/README-linux#build-dependencies), más GLEW/GLM/stb del motor
# y los drivers Mesa por software (llvmpipe) para el smoke test "gpu" sin pantalla.
set -euo pipefail
sudo apt-get update
sudo apt-get install -y --no-install-recommends \
    cmake ninja-build pkg-config \
    libglew-dev libglm-dev libstb-dev \
    libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxfixes-dev libxi-dev libxss-dev libxtst-dev \
    libxkbcommon-dev libwayland-dev wayland-protocols libdecor-0-dev \
    libegl-dev libgl-dev libgles-dev libdrm-dev libgbm-dev \
    libdbus-1-dev libudev-dev \
    libasound2-dev libpulse-dev libpipewire-0.3-dev \
    libegl-mesa0 libgl1-mesa-dri libglx-mesa0
