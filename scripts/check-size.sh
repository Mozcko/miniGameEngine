#!/usr/bin/env bash
# Compara el tamaño de los ejecutables contra el presupuesto fijado en cmake/size-budget.txt
# Uso: scripts/check-size.sh <build-dir>
set -euo pipefail

if [[ $# -lt 1 ]]; then
    echo "Uso: $0 <build-dir>" >&2
    exit 1
fi

build_dir="$1"

if [[ ! -d "$build_dir" ]]; then
    echo "Error: el directorio '$build_dir' no existe" >&2
    exit 1
fi

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
budget_file="$repo_root/cmake/size-budget.txt"

if [[ ! -f "$budget_file" ]]; then
    echo "Error: el archivo de presupuestos '$budget_file' no existe" >&2
    exit 1
fi

# Leer presupuestos: <target> <max-bytes>
declare -A budgets
while IFS= read -r line || [[ -n "$line" ]]; do
    # Quitar espacios al inicio y final
    line="$(echo "$line" | sed -e 's/^[[:space:]]*//' -e 's/[[:space:]]*$//')"
    # Ignorar líneas vacías y comentarios
    if [[ -z "$line" || "$line" =~ ^# ]]; then
        continue
    fi
    target="$(echo "$line" | awk '{print $1}')"
    max_bytes="$(echo "$line" | awk '{print $2}')"
    if [[ -n "$target" && -n "$max_bytes" ]]; then
        budgets["$target"]="$max_bytes"
    fi
done < "$budget_file"

targets=(headless_smoke platformer2d sandbox procgen_gallery audio_gallery sdf_glide guide_game c_hello)
over_budget=0
found_count=0

table=""
table+="| Binary | Size (bytes) | Budget (bytes) | Delta | Status |\n"
table+="| :--- | :---: | :---: | :---: | :---: |\n"

for target in "${targets[@]}"; do
    bin_path="$build_dir/$target"
    if [[ ! -f "$bin_path" ]]; then
        continue
    fi
    found_count=$((found_count + 1))
    size=$(wc -c < "$bin_path" | tr -d '[:space:]')
    budget="${budgets[$target]:-}"
    if [[ -z "$budget" ]]; then
        table+="| \`$target\` | $size | N/A | N/A | ⚠️ No budget |\n"
    else
        delta=$((size - budget))
        if (( delta > 0 )); then
            over_budget=1
            table+="| \`$target\` | $size | $budget | +$delta | ❌ Exceeded |\n"
        else
            table+="| \`$target\` | $size | $budget | $delta | ✅ OK |\n"
        fi
    fi
done

if (( found_count == 0 )); then
    echo "Error: no se encontraron binarios en '$build_dir'" >&2
    exit 1
fi

printf "%b" "$table"

if [[ -n "${GITHUB_STEP_SUMMARY:-}" ]]; then
    printf "### Binary Size Budget Check\n\n%b\n" "$table" >> "$GITHUB_STEP_SUMMARY"
fi

if (( over_budget )); then
    echo -e "\nError: uno o más binarios superan el presupuesto establecido en cmake/size-budget.txt" >&2
    exit 1
fi

exit 0
