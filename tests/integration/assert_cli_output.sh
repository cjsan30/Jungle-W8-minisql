#!/usr/bin/env bash
set -euo pipefail

executable=""
sql_file=""
expected_file=""
expected_pattern=""
db_root="./tests/fixtures/data"
bench_count=0
expect_failure=0

normalize_text() {
    sed 's/\r$//' | awk 'BEGIN { last = 0 } { lines[++last] = $0 } END { while (last > 0 && lines[last] == "") { last-- } for (i = 1; i <= last; i++) { print lines[i] } }'
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        -Executable)
            executable="$2"
            shift 2
            ;;
        -SqlFile)
            sql_file="$2"
            shift 2
            ;;
        -ExpectedFile)
            expected_file="$2"
            shift 2
            ;;
        -ExpectedPattern)
            expected_pattern="$2"
            shift 2
            ;;
        -DbRoot)
            db_root="$2"
            shift 2
            ;;
        -BenchCount)
            bench_count="$2"
            shift 2
            ;;
        -ExpectFailure)
            expect_failure=1
            shift
            ;;
        *)
            printf 'unknown argument: %s\n' "$1" >&2
            exit 1
            ;;
    esac
done

if [ -z "$executable" ]; then
    printf 'missing -Executable\n' >&2
    exit 1
fi

command_args=()
if [ -n "$sql_file" ]; then
    command_args+=("--sql" "$sql_file")
fi
if [ "$bench_count" -gt 0 ]; then
    command_args+=("--bench" "$bench_count")
fi
command_args+=("--db" "$db_root")

set +e
output="$("$executable" "${command_args[@]}" 2>&1)"
status=$?
set -e

normalized_output="$(printf '%s' "$output" | normalize_text)"

if [ "$expect_failure" -eq 1 ]; then
    if [ "$status" -eq 0 ]; then
        printf 'Expected command to fail but it succeeded.\n' >&2
        exit 1
    fi
    if [ -n "$expected_pattern" ] && ! printf '%s\n' "$normalized_output" | grep -Eq "$expected_pattern"; then
        printf "Expected failure output to match pattern '%s' but got:\n%s\n" "$expected_pattern" "$normalized_output" >&2
        exit 1
    fi
    exit 0
fi

if [ "$status" -ne 0 ]; then
    printf '%s\n' "$normalized_output" >&2
    exit "$status"
fi

expected_text="$(normalize_text < "$expected_file")"
if [ "$normalized_output" != "$expected_text" ]; then
    printf 'Unexpected output:\n%s\n' "$normalized_output" >&2
    exit 1
fi
