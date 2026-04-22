#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd "$(dirname "$0")/../.." && pwd)"
db_root="${SQL_TEST_DB_ROOT:-$root_dir/tests/fixtures/data}"
output="$("$root_dir/sql_processor" --bench 10 --db "$db_root" 2>&1)"

if ! printf '%s\n' "$output" | grep -Eq '^benchmark rows=2 iterations=10$'; then
    printf 'Missing benchmark summary:\n%s\n' "$output" >&2
    exit 1
fi
if ! printf '%s\n' "$output" | grep -Eq '^load_build_ms=[0-9]+(\.[0-9]+)?$'; then
    printf 'Missing load_build_ms line:\n%s\n' "$output" >&2
    exit 1
fi
if ! printf '%s\n' "$output" | grep -Eq '^index_hits=10$'; then
    printf 'Missing index_hits line:\n%s\n' "$output" >&2
    exit 1
fi
if ! printf '%s\n' "$output" | grep -Eq '^linear_hits=10$'; then
    printf 'Missing linear_hits line:\n%s\n' "$output" >&2
    exit 1
fi
if ! printf '%s\n' "$output" | grep -Eq '^distinct_probes=[0-9]+$'; then
    printf 'Missing distinct_probes line:\n%s\n' "$output" >&2
    exit 1
fi
if ! printf '%s\n' "$output" | grep -Eq '^avg_linear_steps=[0-9]+(\.[0-9]+)?$'; then
    printf 'Missing avg_linear_steps line:\n%s\n' "$output" >&2
    exit 1
fi
if ! printf '%s\n' "$output" | grep -Eq '^max_linear_steps=[0-9]+$'; then
    printf 'Missing max_linear_steps line:\n%s\n' "$output" >&2
    exit 1
fi
if ! printf '%s\n' "$output" | grep -Eq '^index_ms=[0-9]+(\.[0-9]+)?$'; then
    printf 'Missing index_ms line:\n%s\n' "$output" >&2
    exit 1
fi
if ! printf '%s\n' "$output" | grep -Eq '^linear_ms=[0-9]+(\.[0-9]+)?$'; then
    printf 'Missing linear_ms line:\n%s\n' "$output" >&2
    exit 1
fi
if ! printf '%s\n' "$output" | grep -Eq '^speedup_ratio=[0-9]+(\.[0-9]+)?$'; then
    printf 'Missing speedup_ratio line:\n%s\n' "$output" >&2
    exit 1
fi
