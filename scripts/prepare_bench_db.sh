#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd "$(dirname "$0")/.." && pwd)"
row_count="${1:-1000000}"
sandbox_dir="${2:-$root_dir/.tmp/bench_db}"
csv_file="$sandbox_dir/data/tables/users.csv"

rm -rf "$sandbox_dir"
mkdir -p "$sandbox_dir"
cp -r "$root_dir/data" "$sandbox_dir/data"

bash "$root_dir/scripts/generate_bench_csv.sh" "$row_count" "$csv_file" >/dev/null

printf 'Benchmark DB ready: %s/data\n' "$sandbox_dir"
printf 'Rows prepared: %s\n' "$row_count"
