#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd "$(dirname "$0")/.." && pwd)"

git -C "$root_dir" config --local core.hooksPath .githooks
chmod +x "$root_dir/.githooks/pre-commit" "$root_dir/.githooks/pre-push"

printf 'Installed git hooks with core.hooksPath=.githooks\n'
printf 'Note: local hooks can be bypassed with --no-verify; enforce CI and branch protection on the remote as well.\n'
