#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd "$(dirname "$0")/.." && pwd)"
row_count="${1:-1000000}"
iterations="${2:-10000}"
sandbox_dir="${3:-$root_dir/.tmp/bench_db}"

bash "$root_dir/scripts/prepare_bench_db.sh" "$row_count" "$sandbox_dir"
"$root_dir/sql_processor" --bench "$iterations" --db "$sandbox_dir/data"
