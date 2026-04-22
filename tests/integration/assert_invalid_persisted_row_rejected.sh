#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd "$(dirname "$0")/../.." && pwd)"
tmp_dir="$(mktemp -d)"
trap 'rm -rf "$tmp_dir"' EXIT

cp -r "$root_dir/data" "$tmp_dir/data"
printf '"x","Carol","25"\n' >> "$tmp_dir/data/tables/users.csv"

set +e
output="$("$root_dir/sql_processor" --query "SELECT * FROM users;" --db "$tmp_dir/data" 2>&1)"
status=$?
set -e

if [ "$status" -eq 0 ]; then
    printf 'Expected invalid persisted row to fail but it succeeded.\n' >&2
    exit 1
fi

if ! printf '%s\n' "$output" | grep -Eq 'invalid persisted int for column id'; then
    printf 'Unexpected persisted-row validation output:\n%s\n' "$output" >&2
    exit 1
fi
