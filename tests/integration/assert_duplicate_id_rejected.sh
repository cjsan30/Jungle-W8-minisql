#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd "$(dirname "$0")/../.." && pwd)"
fixture_db_root="${SQL_TEST_DB_ROOT:-$root_dir/tests/fixtures/data}"
tmp_dir="$(mktemp -d)"
trap 'rm -rf "$tmp_dir"' EXIT

cp -r "$fixture_db_root" "$tmp_dir/data"
printf "INSERT INTO users VALUES (2, 'Dup', 99);\n" > "$tmp_dir/dup.sql"

set +e
output="$("$root_dir/sql_processor" --sql "$tmp_dir/dup.sql" --db "$tmp_dir/data" 2>&1)"
status=$?
set -e

if [ "$status" -eq 0 ]; then
    printf 'Expected duplicate-id insert to fail but it succeeded.\n' >&2
    exit 1
fi

if ! printf '%s\n' "$output" | grep -Eq 'duplicate id: 2'; then
    printf 'Unexpected duplicate-id failure output:\n%s\n' "$output" >&2
    exit 1
fi
