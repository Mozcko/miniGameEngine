#!/usr/bin/env bash
# CI dependencies (Ubuntu 24.04). SDL3 is not packaged in 24.04, so it is built from
# source (FetchContent): listed here are its Linux development headers
# (https://wiki.libsdl.org/SDL3/README-linux#build-dependencies), plus GLM/stb for the engine
# and the Mesa software driver for headless "gpu" tests: lavapipe (Vulkan, for SDL3 GPU),
# with validation layers (GPU tests fail on any API usage error).
# glslang-tools compiles shaders (GLSL → SPIR-V, #29). libegl/libgl are headers SDL needs to build.
# fonts-dejavu-core: only for the TTF font test (#41); the engine does not require font files.
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
    glslang-tools fonts-dejavu-core
