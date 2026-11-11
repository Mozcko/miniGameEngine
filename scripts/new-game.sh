#!/usr/bin/env bash
# Crea un juego nuevo a partir de una plantilla de templates/.
# Uso: scripts/new-game.sh [nombre] [--template <3d|2d|headless|window>] [--headless] [--dir <carpeta>]
#   scripts/new-game.sh space_shooter
#   scripts/new-game.sh space_shooter --template 3d
#   scripts/new-game.sh arcade_plane --template 2d
#   scripts/new-game.sh simulation --headless
#   scripts/new-game.sh --template 3d --dir /tmp/my_game
set -euo pipefail
cd "$(dirname "$0")/.."

usage() {
    echo "Uso: scripts/new-game.sh [nombre] [--template <3d|2d|headless|window>] [--headless] [--dir <carpeta>]" >&2
    exit 1
}

name=""
template="3d"
target_dir=""
has_template_flag=false

while [[ $# -gt 0 ]]; do
    case "$1" in
        --template)
            [[ $# -ge 2 ]] || usage
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
            [[ $# -ge 2 ]] || usage
            target_dir="$2"
            shift 2
            ;;
        -h|--help) usage ;;
        -*) echo "Opción desconocida: $1" >&2; usage ;;
        *)
            if [[ -z "$name" ]]; then
                name="$1"
            else
                usage
            fi
            shift
            ;;
    esac
done

if [[ -z "$target_dir" ]]; then
    [[ -n "$name" ]] || usage
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

if [[ ! "$name" =~ ^[a-z][a-z0-9_]*$ ]]; then
    echo "Nombre inválido '$name': usar minúsculas, números y _ (ej: space_shooter)" >&2
    exit 1
fi
for reserved in sandbox run mge_core mge_main mge_platform mge_gpu mge_graphics mge_procgen mge_voxel mge_physics mge_synth mge_audio; do
    [[ "$name" != "$reserved" ]] || { echo "El nombre '$name' está reservado" >&2; exit 1; }
done

tmpl_dir="templates/$template"
if [[ ! -d "$tmpl_dir" ]]; then
    echo "No existe la plantilla '$tmpl_dir'" >&2
    exit 1
fi

if [[ -e "$dest" ]]; then
    echo "Ya existe '$dest'; no se sobrescribe nada" >&2
    exit 1
fi
mkdir -p "$dest"

# space_shooter -> SpaceShooter (clase) y "Space Shooter" (título)
class_name=$(echo "$name" | awk -F_ '{for (i = 1; i <= NF; i++) printf "%s", toupper(substr($i, 1, 1)) substr($i, 2)}')
title=$(echo "$name" | awk -F_ '{for (i = 1; i <= NF; i++) printf "%s%s", (i > 1 ? " " : ""), toupper(substr($i, 1, 1)) substr($i, 2)}')
version=$(grep -m1 "project(miniGameEngine VERSION" CMakeLists.txt 2>/dev/null | awk '{print $3}' || echo "0.9.0")
[[ -n "$version" ]] || version="0.9.0"

# Recorrer y renderizar todos los archivos de la plantilla recursivamente
while IFS= read -r src_file; do
    rel_path="${src_file#$tmpl_dir/}"

    # Reemplazar placeholders en el path relativo de destino
    target_rel=$(echo "$rel_path" | sed \
        -e "s/@GAME_CLASS@/$class_name/g" \
        -e "s/@GAME_NAME@/$name/g" \
        -e "s/Game\.cpp/$class_name.cpp/g" \
        -e "s/\.in$//")

    dest_file="$dest/$target_rel"
    mkdir -p "$(dirname "$dest_file")"

    if [[ "$src_file" == *.in ]]; then
        sed \
            -e "s/@GAME_NAME@/$name/g" \
            -e "s/@GAME_CLASS@/$class_name/g" \
            -e "s/@GAME_TITLE@/$title/g" \
            -e "s/@MGE_VERSION@/$version/g" \
            "$src_file" > "$dest_file"
    else
        cp "$src_file" "$dest_file"
    fi

    # Formatear archivos C++ generados si clang-format está disponible
    if [[ "$dest_file" =~ \.(cpp|h)$ ]] && command -v clang-format >/dev/null 2>&1; then
        clang-format -i "$dest_file"
    fi
done < <(find "$tmpl_dir" -type f)

# Registrar el juego en el CMakeLists de la carpeta destino (si es una subcarpeta dentro del repo con CMakeLists)
parent_cmake="$(dirname "$dest")/CMakeLists.txt"
if [[ -f "$parent_cmake" && "$parent_cmake" != "$dest/CMakeLists.txt" ]]; then
    if ! grep -q "add_subdirectory($name)" "$parent_cmake" 2>/dev/null; then
        echo "add_subdirectory($name)" >> "$parent_cmake"
    fi
    registered="Registrado en $parent_cmake."
else
    registered="Proyecto independiente generado en $dest."
fi

cat <<MSG
Creado '$dest' (plantilla $template, clase $class_name). $registered

Siguientes pasos (proyecto independiente):
  cd "$dest"
  cmake --preset dev
  cmake --build --preset dev
  ctest --preset dev

Siguientes pasos (si compila desde la raíz del motor):
  cmake --preset dev
  cmake --build --preset dev --target run-$name
  ctest --preset dev -R $name
MSG
