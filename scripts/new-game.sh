#!/usr/bin/env bash
# Crea un juego nuevo a partir de una plantilla de templates/.
# Uso: scripts/new-game.sh <nombre> [--headless] [--dir <carpeta>]   (carpeta por defecto: examples)
#   scripts/new-game.sh space_shooter
#   scripts/new-game.sh simulation --headless
set -euo pipefail
cd "$(dirname "$0")/.."

usage() {
    echo "Uso: scripts/new-game.sh <nombre> [--headless] [--dir <carpeta>]" >&2
    exit 1
}

name=""
template="window"
target_dir="examples"
while [[ $# -gt 0 ]]; do
    case "$1" in
        --headless) template="headless"; shift ;;
        --dir) [[ $# -ge 2 ]] || usage; target_dir="$2"; shift 2 ;;
        -h|--help) usage ;;
        -*) echo "Opción desconocida: $1" >&2; usage ;;
        *) [[ -z "$name" ]] || usage; name="$1"; shift ;;
    esac
done
[[ -n "$name" ]] || usage

if [[ ! "$name" =~ ^[a-z][a-z0-9_]*$ ]]; then
    echo "Nombre inválido '$name': usar minúsculas, números y _ (ej: space_shooter)" >&2
    exit 1
fi
for reserved in sandbox run mge_core mge_main mge_platform mge_render mge_audio; do
    [[ "$name" != "$reserved" ]] || { echo "El nombre '$name' está reservado" >&2; exit 1; }
done

# space_shooter -> SpaceShooter (clase) y "Space Shooter" (título)
class_name=$(echo "$name" | awk -F_ '{for (i = 1; i <= NF; i++) printf "%s", toupper(substr($i, 1, 1)) substr($i, 2)}')
title=$(echo "$name" | awk -F_ '{for (i = 1; i <= NF; i++) printf "%s%s", (i > 1 ? " " : ""), toupper(substr($i, 1, 1)) substr($i, 2)}')

dest="$target_dir/$name"
if [[ -e "$dest" ]]; then
    echo "Ya existe '$dest'; no se sobrescribe nada" >&2
    exit 1
fi
mkdir -p "$dest"

render() {
    sed -e "s/@GAME_NAME@/$name/g" -e "s/@GAME_CLASS@/$class_name/g" -e "s/@GAME_TITLE@/$title/g" "$1" > "$2"
}
render "templates/$template/Game.cpp.in" "$dest/$class_name.cpp"
render "templates/$template/CMakeLists.txt.in" "$dest/CMakeLists.txt"
render "templates/$template/README.md.in" "$dest/README.md"

# Nombres largos cambian el largo de las líneas: dejar el código con el formato del proyecto
if command -v clang-format >/dev/null 2>&1; then
    clang-format -i "$dest/$class_name.cpp"
fi

# Registrar el juego en el CMakeLists de la carpeta (si existe)
parent_cmake="$target_dir/CMakeLists.txt"
if [[ -f "$parent_cmake" ]]; then
    grep -qx "add_subdirectory($name)" "$parent_cmake" || echo "add_subdirectory($name)" >> "$parent_cmake"
    registered="Registrado en $parent_cmake."
else
    registered="Agregar 'add_subdirectory($name)' al CMakeLists.txt que incluya '$target_dir'."
fi

cat <<MSG
Creado '$dest' (plantilla $template, clase $class_name). $registered

Siguientes pasos:
  cmake --preset dev
  cmake --build --preset dev --target run-$name
  ctest --preset dev -R $name
MSG
