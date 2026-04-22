#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd "$(dirname "$0")/.." && pwd)"
sandbox_dir="${1:-$root_dir/.tmp/manual_db}"

rm -rf "$sandbox_dir"
mkdir -p "$sandbox_dir"
cp -r "$root_dir/data" "$sandbox_dir/data"

cat > "$sandbox_dir/README.txt" <<EOF
Manual DB sandbox created.

Sandbox DB root:
$sandbox_dir/data

Example commands:
./sql_processor --sql examples/select_all.sql --db "$sandbox_dir/data"
./sql_processor --sql examples/select_by_id.sql --db "$sandbox_dir/data"
./sql_processor --sql examples/insert_user.sql --db "$sandbox_dir/data"
./sql_processor --sql examples/insert_and_select.sql --db "$sandbox_dir/data"

You can also copy and edit the example SQL files for your own checks.
EOF

printf 'Sandbox ready: %s\n' "$sandbox_dir/data"
printf 'Example SQL files: %s/examples\n' "$root_dir"
