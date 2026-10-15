#!/usr/bin/env bash
# Mismos checks que el job "lint" de CI: formato (clang-format) y nombres (clang-tidy).
# Uso: scripts/lint.sh [--fix] [build-dir]   (build-dir por defecto: build-ci, debe tener compile_commands.json)
set -euo pipefail
cd "$(dirname "$0")/.."

fix=0
if [[ "${1:-}" == "--fix" ]]; then fix=1; shift; fi
build_dir="${1:-build-ci}"

# Archivos versionados + nuevos sin ignorar (para que los archivos nuevos también se revisen)
mapfile -t sources < <(git ls-files -co --exclude-standard '*.h' '*.cpp')
mapfile -t units < <(git ls-files -co --exclude-standard '*.cpp')

if (( fix )); then
    clang-format -i "${sources[@]}"
fi
echo "== clang-format"
clang-format --dry-run --Werror "${sources[@]}"

echo "== clang-tidy (naming)"
if [[ ! -f "$build_dir/compile_commands.json" ]]; then
    echo "Falta $build_dir/compile_commands.json: ejecuta 'cmake --preset ci' primero" >&2
    exit 1
fi
printf '%s\n' "${units[@]}" | xargs -P "$(nproc)" -n 1 clang-tidy -p "$build_dir" --quiet
echo "OK"
