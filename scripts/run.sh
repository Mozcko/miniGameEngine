#!/usr/bin/env bash
# Builds and runs the game in one step, from any directory.
# Usage: scripts/run.sh [preset] [-- game arguments]     (default preset: dev)
#   MGE_MAX_FRAMES=300 scripts/run.sh          -> exits after 300 frames
#   SDL_VIDEODRIVER=offscreen scripts/run.sh   -> headless (no window)
set -euo pipefail
cd "$(dirname "$0")/.."

preset="dev"
if [[ $# -gt 0 && "$1" != "--" ]]; then preset="$1"; shift; fi
if [[ "${1:-}" == "--" ]]; then shift; fi

# Build directory for each preset (see CMakePresets.json)
case "$preset" in
    dev) build_dir="build" ;;
    *) build_dir="build-$preset" ;;
esac

if [[ ! -f "$build_dir/CMakeCache.txt" ]]; then
    cmake --preset "$preset"
fi
cmake --build --preset "$preset" --target sandbox
exec "$build_dir/sandbox" "$@"
