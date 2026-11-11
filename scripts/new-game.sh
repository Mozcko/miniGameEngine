#!/usr/bin/env bash
# Wrapper sobre 'mge new' para crear juegos nuevos a partir de plantillas.
# Uso: scripts/new-game.sh [nombre] [--template <3d|2d|headless|window>] [--headless] [--dir <carpeta>]
#   scripts/new-game.sh space_shooter
#   scripts/new-game.sh space_shooter --template 3d
#   scripts/new-game.sh arcade_plane --template 2d
#   scripts/new-game.sh simulation --headless
#   scripts/new-game.sh --template 3d --dir /tmp/my_game
set -euo pipefail
cd "$(dirname "$0")/.."

usage() {
    echo "Uso: scripts/new-game.sh [nombre] [--template <3d|2d|headless|window>] [--headless] [--dir <carpeta>]"
}

name=""
template="3d"
target_dir=""
has_template_flag=false

while [[ $# -gt 0 ]]; do
    case "$1" in
        --template)
            [[ $# -ge 2 ]] || { usage >&2; exit 1; }
            template="$2"
            has_template_flag=true
            shift 2
            ;;
        --headless)
            template="headless"
            has_template_flag=true
            shift
            ;;
        --dir)
            [[ $# -ge 2 ]] || { usage >&2; exit 1; }
            target_dir="$2"
            shift 2
            ;;
        -h|--help) usage; exit 0 ;;
        -*) echo "Opción desconocida: $1" >&2; usage >&2; exit 1 ;;
        *)
            if [[ -z "$name" ]]; then
                name="$1"
            else
                usage >&2
                exit 1
            fi
            shift
            ;;
    esac
done

if [[ -z "$target_dir" ]]; then
    [[ -n "$name" ]] || { usage >&2; exit 1; }
    target_dir="examples"
    dest="$target_dir/$name"
else
    if [[ -z "$name" ]]; then
        name=$(basename "$target_dir")
        dest="$target_dir"
    elif [[ "$(basename "$target_dir")" == "$name" ]]; then
        dest="$target_dir"
    else
        dest="$target_dir/$name"
    fi
fi

# Compatibilidad con ci_window sin flag --template
if [[ "$name" == "ci_window" && "$has_template_flag" == false ]]; then
    template="window"
fi

# Localizar el ejecutable mge
mge_bin=""
for candidate in "./build/mge" "./build-ci/mge" "./build-release/mge"; do
    if [[ -x "$candidate" ]]; then
        mge_bin="$candidate"
        break
    fi
done

if [[ -z "$mge_bin" ]]; then
    if command -v mge >/dev/null 2>&1; then
        mge_bin="mge"
    else
        echo "Compilando herramienta mge CLI..."
        if [[ -d "build" ]]; then
            cmake --build build --target mge
            mge_bin="./build/mge"
        elif [[ -d "build-ci" ]]; then
            cmake --build build-ci --target mge
            mge_bin="./build-ci/mge"
        else
            cmake --preset dev
            cmake --build --preset dev --target mge
            mge_bin="./build/mge"
        fi
    fi
fi

exec "$mge_bin" new "$name" --template "$template" --dir "$dest"
