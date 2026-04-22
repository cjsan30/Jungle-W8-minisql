#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd "$(dirname "$0")/../.." && pwd)"
tmp_dir="$(mktemp -d)"
trap 'rm -rf "$tmp_dir"' EXIT

cp -r "$root_dir/data" "$tmp_dir/data"
printf "INSERT INTO users VALUES ('A,B', 25);\n" > "$tmp_dir/insert.sql"

insert_output="$("$root_dir/sql_processor" --sql "$tmp_dir/insert.sql" --db "$tmp_dir/data" 2>&1)"
if [ "$insert_output" != "INSERT 1" ]; then
    printf 'Unexpected insert output:\n%s\n' "$insert_output" >&2
    exit 1
fi

select_output="$("$root_dir/sql_processor" --query "SELECT * FROM users WHERE id = ?;" --param 3 --db "$tmp_dir/data" 2>&1)"

if ! grep -Fxq '"3","A,B","25"' "$tmp_dir/data/tables/users.csv"; then
    printf 'Persisted CSV row was not correctly quoted:\n' >&2
    cat "$tmp_dir/data/tables/users.csv" >&2
    exit 1
fi

if ! printf '%s\n' "$select_output" | grep -Fxq 'id,name,age'; then
    printf 'Missing select header:\n%s\n' "$select_output" >&2
    exit 1
fi
if ! printf '%s\n' "$select_output" | grep -Fxq '3,A,B,25'; then
    printf 'Unexpected select output:\n%s\n' "$select_output" >&2
    exit 1
fi
