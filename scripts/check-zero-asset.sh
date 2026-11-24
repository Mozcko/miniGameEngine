#!/usr/bin/env bash
# Verifies that procedural binaries run from a clean directory without assets (ADR 0004, #65).
# Usage: scripts/check-zero-asset.sh <build-dir> [--require-strace]
set -euo pipefail

if [[ $# -lt 1 ]]; then
    echo "Usage: $0 <build-dir> [--require-strace]" >&2
    exit 1
fi

build_dir="$1"
require_strace=0
if [[ "${2:-}" == "--require-strace" ]]; then
    require_strace=1
fi

if [[ ! -d "$build_dir" ]]; then
    echo "Error: directory '$build_dir' does not exist" >&2
    exit 1
fi

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
repo_assets="$repo_root/assets"

has_strace=0
if command -v strace >/dev/null 2>&1; then
    has_strace=1
elif (( require_strace )); then
    echo "Error: strace is required but not installed" >&2
    exit 1
else
    echo "Warning: strace is not installed; skipping openat syscall verification" >&2
fi

# Binaries to verify in procedural mode (no assets)
targets=(headless_smoke sandbox procgen_gallery sdf_glide guide_game c_hello)

for target in "${targets[@]}"; do
    bin_path="$build_dir/$target"
    if [[ ! -f "$bin_path" ]]; then
        echo "Error: executable '$target' not found in '$build_dir'" >&2
        exit 1
    fi
done

# Create temporary directory OUTSIDE the repository
tmp_dir="$(mktemp -d /tmp/mge-zero-asset-XXXXXX)"
trap 'rm -rf "$tmp_dir"' EXIT

echo "Temporary test directory: $tmp_dir"

# Copy ONLY the executables
for target in "${targets[@]}"; do
    cp "$build_dir/$target" "$tmp_dir/"
done

cd "$tmp_dir"

# Environment variables required for execution
export SDL_VIDEODRIVER=offscreen
export SDL_AUDIODRIVER=dummy
export MGE_MAX_FRAMES=120
# VK_DRIVER_FILES if lavapipe is available for local headless environments without a display
if [[ -z "${VK_DRIVER_FILES:-}" && -f "/usr/share/vulkan/icd.d/lvp_icd.x86_64.json" ]]; then
    export VK_DRIVER_FILES="/usr/share/vulkan/icd.d/lvp_icd.x86_64.json"
fi
unset MGE_ASSET_DIR || true
unset SANDBOX_ATLAS || true

failed=0

for target in "${targets[@]}"; do
    echo "--- Checking $target in zero-asset mode ---"
    run_log="$tmp_dir/run_${target}.log"
    strace_log="$tmp_dir/strace_${target}.log"

    set +e
    if (( has_strace )); then
        strace -f -e trace=openat -o "$strace_log" "./$target" > "$run_log" 2>&1
        exit_code=$?
    else
        "./$target" > "$run_log" 2>&1
        exit_code=$?
    fi
    set -e

    if (( exit_code != 0 )); then
        echo "❌ Error: $target exited with status $exit_code" >&2
        cat "$run_log" >&2
        failed=1
        continue
    fi

    # Verify the log contains no Assets warnings or errors
    if grep -i -E "\[(WARN |ERROR)\].*\[Assets\]|\[Assets\].*\[(WARN |ERROR)\]|Assets.*(WARN|ERROR)" "$run_log"; then
        echo "❌ Error: $target emitted Assets-related warnings or errors in the log:" >&2
        grep -i -E "\[(WARN |ERROR)\].*\[Assets\]|\[Assets\].*\[(WARN |ERROR)\]|Assets.*(WARN|ERROR)" "$run_log" >&2
        failed=1
        continue
    fi

    # Verify no attempt was made to open any file under assets/ (relative or absolute, success or ENOENT)
    if (( has_strace )); then
        offending="$(grep -E 'openat\([^,]+,\s*"([^"]*/)?assets/' "$strace_log" || true)"
        if [[ -n "$offending" ]]; then
            echo "❌ Error: $target attempted to open files under assets/:" >&2
            echo "$offending" >&2
            failed=1
            continue
        fi
        if grep -F "$repo_assets" "$strace_log" >/dev/null 2>&1; then
            echo "❌ Error: $target attempted to open files inside '$repo_assets':" >&2
            grep -F "$repo_assets" "$strace_log" >&2
            failed=1
            continue
        fi
    fi

    echo "✅ $target passed zero-asset check"
done

if (( failed )); then
    echo "Error: one or more zero-asset checks failed" >&2
    exit 1
fi

echo "All zero-asset checks passed."
exit 0
