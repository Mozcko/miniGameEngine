#!/usr/bin/env bash
# Comprueba que los binarios procedurales corran desde un directorio limpio sin assets (ADR 0004, #65).
# Uso: scripts/check-zero-asset.sh <build-dir> [--require-strace]
set -euo pipefail

if [[ $# -lt 1 ]]; then
    echo "Uso: $0 <build-dir> [--require-strace]" >&2
    exit 1
fi

build_dir="$1"
require_strace=0
if [[ "${2:-}" == "--require-strace" ]]; then
    require_strace=1
fi

if [[ ! -d "$build_dir" ]]; then
    echo "Error: el directorio '$build_dir' no existe" >&2
    exit 1
fi

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
repo_assets="$repo_root/assets"

has_strace=0
if command -v strace >/dev/null 2>&1; then
    has_strace=1
elif (( require_strace )); then
    echo "Error: strace es requerido pero no está instalado" >&2
    exit 1
else
    echo "Aviso: strace no está instalado; se omitirá la verificación de llamadas al sistema openat" >&2
fi

# Binarios a verificar en modo procedural (sin assets)
targets=(headless_smoke sandbox procgen_gallery sdf_glide)

for target in "${targets[@]}"; do
    bin_path="$build_dir/$target"
    if [[ ! -f "$bin_path" ]]; then
        echo "Error: no se encontró el ejecutable '$target' en '$build_dir'" >&2
        exit 1
    fi
done

# Crear directorio temporal FUERA del repositorio
tmp_dir="$(mktemp -d /tmp/mge-zero-asset-XXXXXX)"
trap 'rm -rf "$tmp_dir"' EXIT

echo "Directorio de prueba temporal: $tmp_dir"

# Copiar ÚNICAMENTE los ejecutables
for target in "${targets[@]}"; do
    cp "$build_dir/$target" "$tmp_dir/"
done

cd "$tmp_dir"

# Variables de entorno requeridas para la ejecución
export SDL_VIDEODRIVER=offscreen
export SDL_AUDIODRIVER=dummy
export MGE_MAX_FRAMES=120
# VK_DRIVER_FILES si lavapipe está disponible para entornos headless locales sin pantalla
if [[ -z "${VK_DRIVER_FILES:-}" && -f "/usr/share/vulkan/icd.d/lvp_icd.x86_64.json" ]]; then
    export VK_DRIVER_FILES="/usr/share/vulkan/icd.d/lvp_icd.x86_64.json"
fi
unset MGE_ASSET_DIR || true
unset SANDBOX_ATLAS || true

failed=0

for target in "${targets[@]}"; do
    echo "--- Verificando $target en modo zero-asset ---"
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
        echo "❌ Error: $target terminó con código de salida $exit_code" >&2
        cat "$run_log" >&2
        failed=1
        continue
    fi

    # Verificar que el log no contenga advertencias o errores de Assets
    if grep -i -E "\[(WARN |ERROR)\].*\[Assets\]|\[Assets\].*\[(WARN |ERROR)\]|Assets.*(WARN|ERROR)" "$run_log"; then
        echo "❌ Error: $target produjo advertencias o errores relacionados con Assets en el log:" >&2
        grep -i -E "\[(WARN |ERROR)\].*\[Assets\]|\[Assets\].*\[(WARN |ERROR)\]|Assets.*(WARN|ERROR)" "$run_log" >&2
        failed=1
        continue
    fi

    # Verificar que no se haya abierto ningún archivo bajo el directorio assets/ del repo
    if (( has_strace )); then
        if grep -F "$repo_assets" "$strace_log"; then
            echo "❌ Error: $target intentó abrir archivos dentro de '$repo_assets':" >&2
            grep -F "$repo_assets" "$strace_log" >&2
            failed=1
            continue
        fi
    fi

    echo "✅ $target pasó la verificación zero-asset"
done

if (( failed )); then
    echo "Error: una o más verificaciones de zero-asset fallaron" >&2
    exit 1
fi

echo "Todas las comprobaciones de zero-asset fueron exitosas."
exit 0
