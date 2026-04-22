#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd "$(dirname "$0")/../.." && pwd)"
fixture_db_root="${SQL_TEST_DB_ROOT:-$root_dir/tests/fixtures/data}"
tmp_dir="$(mktemp -d)"
trap 'rm -rf "$tmp_dir"' EXIT

cp -r "$fixture_db_root" "$tmp_dir/data"
cat > "$tmp_dir/script.sql" <<'EOF'
-- insert a new user
INSERT INTO users VALUES ('Carol', 25);

-- retrieve through index path
SELECT * FROM users WHERE id = 3;
EOF

output="$("$root_dir/sql_processor" --sql "$tmp_dir/script.sql" --db "$tmp_dir/data" 2>&1)"
expected_output="$(cat <<'EOF'
INSERT 1
id,name,age
3,Carol,25
EOF
)"

if [ "$output" != "$expected_output" ]; then
    printf 'Unexpected commented-script output:\n%s\n' "$output" >&2
    exit 1
fi
