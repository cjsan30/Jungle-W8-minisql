#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd "$(dirname "$0")/../.." && pwd)"

if [ ! -f "$root_dir/.githooks/pre-commit" ]; then
    printf 'Missing .githooks/pre-commit\n' >&2
    exit 1
fi

if [ ! -f "$root_dir/.githooks/pre-push" ]; then
    printf 'Missing .githooks/pre-push\n' >&2
    exit 1
fi

if ! grep -Fq 'make test' "$root_dir/.githooks/pre-commit"; then
    printf 'pre-commit must run make test\n' >&2
    exit 1
fi

if ! grep -Fq 'make test' "$root_dir/.githooks/pre-push"; then
    printf 'pre-push must run make test\n' >&2
    exit 1
fi

hooks_path="$(git -C "$root_dir" config --local --get core.hooksPath || true)"
if [ "$hooks_path" != ".githooks" ]; then
    printf 'core.hooksPath must be .githooks, got: %s\n' "$hooks_path" >&2
    exit 1
fi
