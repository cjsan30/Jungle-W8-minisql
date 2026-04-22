#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd "$(dirname "$0")/../.." && pwd)"
tmp_dir="$(mktemp -d)"
trap 'rm -rf "$tmp_dir"' EXIT

cp -r "$root_dir/data" "$tmp_dir/data"
printf "INSERT INTO users VALUES ('Carol', 25);\n" > "$tmp_dir/insert.sql"
printf "SELECT * FROM users WHERE id = 3;\n" > "$tmp_dir/select.sql"

insert_output="$("$root_dir/sql_processor" --sql "$tmp_dir/insert.sql" --db "$tmp_dir/data" 2>&1)"
if [ "$insert_output" != "INSERT 1" ]; then
    printf 'Unexpected insert output:\n%s\n' "$insert_output" >&2
    exit 1
fi

select_output="$("$root_dir/sql_processor" --sql "$tmp_dir/select.sql" --db "$tmp_dir/data" 2>&1)"
expected_output="$(cat <<'EOF'
id,name,age
3,Carol,25
EOF
)"

if [ "$select_output" != "$expected_output" ]; then
    printf 'Unexpected select output:\n%s\n' "$select_output" >&2
    exit 1
fi
