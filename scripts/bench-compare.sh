#!/usr/bin/env bash
# Compara resultados de benchmark con una línea base JSON
# Uso: scripts/bench-compare.sh <current.json> [baseline.json]
set -euo pipefail

if [[ $# -lt 1 ]]; then
    echo "Uso: $0 <current.json> [baseline.json]" >&2
    exit 0
fi

current_file="$1"
baseline_file="${2:-}"

if [[ ! -f "$current_file" ]]; then
    echo "Aviso: no se encontró '$current_file'" >&2
    exit 0
fi

# Si no hay archivo base o no existe
if [[ -z "$baseline_file" || ! -f "$baseline_file" ]]; then
    table=$(jq -r '
      def list(x): if (x | type) == "array" then x else x.benchmarks end;
      ["| Benchmark | Current (ns) | Iterations | Unit |",
       "| :--- | :---: | :---: | :---: |"] +
      ([ list(.)[] | "| `\(.name)` | \(.medianNs) | \(.iterations) | \(.unit) |" ])
      | join("\n")
    ' "$current_file")

    echo "$table"
    if [[ -n "${GITHUB_STEP_SUMMARY:-}" ]]; then
        printf "### Benchmark Results (No baseline)\n\n%s\n" "$table" >> "$GITHUB_STEP_SUMMARY"
    fi
    exit 0
fi

# Hay archivo base: comparar cada benchmark por nombre
table=$(jq -r -s '
  def list(x): if (x | type) == "array" then x else x.benchmarks end;
  .[0] as $f0 | .[1] as $f1 |
  (list($f0) | map({key: .name, value: .}) | from_entries) as $base |
  list($f1) as $curr |

  ["| Benchmark | Baseline (ns) | Current (ns) | Delta | Status |",
   "| :--- | :---: | :---: | :---: | :---: |"] +
  [
    $curr[] |
    .name as $name |
    .medianNs as $currVal |
    ($base[$name]) as $b |
    if $b == null then
      "| `\($name)` | N/A | \($currVal) | N/A | ℹ️ New |"
    else
      ($b.medianNs) as $baseVal |
      if $baseVal == 0 then
        "| `\($name)` | 0.00 | \($currVal) | N/A | ℹ️ Base 0 |"
      else
        ((($currVal - $baseVal) / $baseVal) * 100) as $delta |
        (if $delta > 0 then "+\($delta * 100 | round / 100)%" else "\($delta * 100 | round / 100)%" end) as $deltaStr |
        (if $delta > 15 then "⚠️ +\(($delta * 100 | round / 100))%" elif $delta < -10 then "🚀 \($deltaStr)" else "✅ OK" end) as $status |
        "| `\($name)` | \($baseVal) | \($currVal) | \($deltaStr) | \($status) |"
      end
    end
  ] | join("\n")
' "$baseline_file" "$current_file")

echo "$table"

if [[ -n "${GITHUB_STEP_SUMMARY:-}" ]]; then
    printf "### Benchmark Comparison\n\n%s\n" "$table" >> "$GITHUB_STEP_SUMMARY"
fi

exit 0
