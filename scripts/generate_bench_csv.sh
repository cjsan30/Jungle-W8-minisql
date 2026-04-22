#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -lt 2 ]; then
    printf 'usage: %s <row_count> <output_csv>\n' "$0" >&2
    exit 1
fi

row_count="$1"
output_csv="$2"

if ! [[ "$row_count" =~ ^[0-9]+$ ]] || [ "$row_count" -lt 1 ]; then
    printf 'row_count must be an integer >= 1\n' >&2
    exit 1
fi

mkdir -p "$(dirname "$output_csv")"

awk -v row_count="$row_count" 'BEGIN {
    for (i = 1; i <= row_count; i++) {
        printf "\"%d\",\"user_%d\",\"%d\"\n", i, i, (i % 100)
    }
}' > "$output_csv"

printf 'Generated %s\n' "$output_csv"
