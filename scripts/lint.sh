#!/usr/bin/env bash
# Same checks as the CI "lint" job: formatting (clang-format) and naming (clang-tidy).
# Usage: scripts/lint.sh [--fix] [build-dir]   (default build-dir: build-ci, must contain compile_commands.json)
set -euo pipefail
cd "$(dirname "$0")/.."

fix=0
if [[ "${1:-}" == "--fix" ]]; then fix=1; shift; fi
build_dir="${1:-build-ci}"

# Tracked files + untracked non-ignored files (so new files are checked too)
mapfile -t sources < <(git ls-files -co --exclude-standard '*.h' '*.cpp')
# samples/ and bindings/ are not part of the default ci preset compile_commands.json
mapfile -t units < <(git ls-files -co --exclude-standard '*.cpp' | grep -Ev '^(samples|bindings)/' || true)

if (( fix )); then
    clang-format -i "${sources[@]}"
fi
echo "== clang-format"
clang-format --dry-run --Werror "${sources[@]}"

echo "== clang-tidy (naming)"
if [[ ! -f "$build_dir/compile_commands.json" ]]; then
    echo "Missing $build_dir/compile_commands.json: run 'cmake --preset ci' first" >&2
    exit 1
fi
# Shader headers are generated at build time: clang-tidy needs them to analyze files that include them
# (the target only exists when GPU modules are enabled)
if grep -q "^MGE_BUILD_MODULES:BOOL=ON" "$build_dir/CMakeCache.txt"; then
    cmake --build "$build_dir" --target mge_generated_shaders >/dev/null
fi
printf '%s\n' "${units[@]}" | xargs -P "$(nproc)" -n 1 clang-tidy -p "$build_dir" --quiet
echo "OK"
