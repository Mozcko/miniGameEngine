#!/usr/bin/env bash
# Dependencias de CI (Ubuntu 24.04). SDL3 no viene empaquetado en 24.04, así que se compila
# desde el código fuente (FetchContent): aquí van sus headers de desarrollo de Linux
# (https://wiki.libsdl.org/SDL3/README-linux#build-dependencies), más GLM/stb del motor
# y el driver Mesa por software para los tests "gpu" sin pantalla: lavapipe (Vulkan, para SDL3 GPU),
# con las capas de validación (los tests de GPU fallan ante cualquier error de uso de la API).
# glslang-tools compila los shaders (GLSL → SPIR-V, #29). libegl/libgl son headers que pide SDL al compilar.
set -euo pipefail
sudo apt-get update
sudo apt-get install -y --no-install-recommends \
    cmake ninja-build pkg-config \
    libglm-dev libstb-dev \
    libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxfixes-dev libxi-dev libxss-dev libxtst-dev \
    libxkbcommon-dev libwayland-dev wayland-protocols libdecor-0-dev \
    libegl-dev libgl-dev libgles-dev libdrm-dev libgbm-dev \
    libdbus-1-dev libudev-dev \
    libasound2-dev libpulse-dev libpipewire-0.3-dev \
    libvulkan1 mesa-vulkan-drivers vulkan-validationlayers \
    glslang-tools
