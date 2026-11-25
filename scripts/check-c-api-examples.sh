#!/usr/bin/env bash
# Builds libmge_c (shared) if needed and verifies the Zig and Odin C API examples (#186).
# Usage: scripts/check-c-api-examples.sh [build-dir] [--require-zig] [--require-odin] [--headless-only]
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="$repo_root/build-c-shared"
require_zig=0
require_odin=0
headless_only=0

for arg in "$@"; do
    case "$arg" in
        --require-zig) require_zig=1 ;;
        --require-odin) require_odin=1 ;;
        --headless-only) headless_only=1 ;;
        --help|-h)
            echo "Usage: $0 [build-dir] [--require-zig] [--require-odin] [--headless-only]"
            exit 0
            ;;
        -*)
            echo "Unknown option: $arg" >&2
            exit 1
            ;;
        *)
            build_dir="$arg"
            ;;
    esac
done

# Resolve build_dir to an absolute path
mkdir -p "$build_dir"
build_dir="$(cd "$build_dir" && pwd)"
lib_dir="$build_dir/src/mge/c"

# Build libmge_c shared library if not already present
if [[ ! -f "$lib_dir/libmge_c.so" && ! -f "$lib_dir/libmge_c.dylib" ]]; then
    echo "== Building mge_c_shared in $build_dir"
    cmake -S "$repo_root" -B "$build_dir" \
        -DCMAKE_BUILD_TYPE=Release \
        -DMGE_BUILD_C_SHARED=ON \
        -DMGE_BUILD_TESTS=OFF \
        -DMGE_BUILD_BENCHMARKS=OFF \
        -DMGE_BUILD_EXAMPLES=OFF \
        -DMGE_BUILD_SANDBOX=OFF
    cmake --build "$build_dir" --target mge_c_shared
fi

if [[ ! -f "$lib_dir/libmge_c.so" && ! -f "$lib_dir/libmge_c.dylib" ]]; then
    echo "Error: libmge_c shared library not found in '$lib_dir'" >&2
    exit 1
fi

export SDL_VIDEODRIVER=offscreen
export SDL_AUDIODRIVER=dummy
export MGE_MAX_FRAMES=60
if [[ -z "${VK_DRIVER_FILES:-}" && -f "/usr/share/vulkan/icd.d/lvp_icd.x86_64.json" ]]; then
    export VK_DRIVER_FILES="/usr/share/vulkan/icd.d/lvp_icd.x86_64.json"
fi

tmp_dir="$(mktemp -d /tmp/mge-c-api-examples-XXXXXX)"
trap 'rm -rf "$tmp_dir"' EXIT

checked=0

# --- Zig example ---
if command -v zig >/dev/null 2>&1; then
    echo "== Building Zig example"
    zig_prefix="$tmp_dir/zig-out"
    (
        cd "$repo_root/examples/zig"
        zig build \
            --prefix "$zig_prefix" \
            -Dmge-include="$repo_root/src" \
            -Dmge-lib="$lib_dir"
    )
    echo "== Running Zig example (headless)"
    "$zig_prefix/bin/zig_hello" --headless
    if (( ! headless_only )); then
        echo "== Running Zig example (offscreen 60 frames)"
        "$zig_prefix/bin/zig_hello"
    fi
    echo "OK: Zig example passed"
    checked=$((checked + 1))
elif (( require_zig )); then
    echo "Error: zig is required (--require-zig) but not installed" >&2
    exit 1
else
    echo "Skipping Zig example (zig not found in PATH)"
fi

# --- Odin example ---
if command -v odin >/dev/null 2>&1; then
    echo "== Building Odin example"
    odin_bin="$tmp_dir/odin_hello"
    (
        cd "$repo_root/examples/odin"
        odin build . \
            -out:"$odin_bin" \
            -extra-linker-flags:"-L${lib_dir} -Wl,-rpath,${lib_dir}"
    )
    echo "== Running Odin example (headless)"
    "$odin_bin" --headless
    if (( ! headless_only )); then
        echo "== Running Odin example (offscreen 60 frames)"
        "$odin_bin"
    fi
    echo "OK: Odin example passed"
    checked=$((checked + 1))
elif (( require_odin )); then
    echo "Error: odin is required (--require-odin) but not installed" >&2
    exit 1
else
    echo "Skipping Odin example (odin not found in PATH)"
fi

if (( checked == 0 )); then
    echo "Note: neither zig nor odin is installed; built mge_c_shared only."
fi
