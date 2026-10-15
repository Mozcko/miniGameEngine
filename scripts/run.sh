#!/usr/bin/env bash
# Compila y ejecuta el juego en un paso, desde cualquier directorio.
# Uso: scripts/run.sh [preset] [-- argumentos del juego]     (preset por defecto: dev)
#   MGE_MAX_FRAMES=300 scripts/run.sh          -> sale tras 300 frames
#   SDL_VIDEODRIVER=offscreen scripts/run.sh   -> sin ventana
set -euo pipefail
cd "$(dirname "$0")/.."

preset="dev"
if [[ $# -gt 0 && "$1" != "--" ]]; then preset="$1"; shift; fi
if [[ "${1:-}" == "--" ]]; then shift; fi

# Carpeta de build de cada preset (ver CMakePresets.json)
case "$preset" in
    dev) build_dir="build" ;;
    *) build_dir="build-$preset" ;;
esac

if [[ ! -f "$build_dir/CMakeCache.txt" ]]; then
    cmake --preset "$preset"
fi
cmake --build --preset "$preset" --target sandbox
exec "$build_dir/sandbox" "$@"
