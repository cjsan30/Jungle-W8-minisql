#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SERVER_BIN="${ROOT_DIR}/sql_api_server"
FIXTURE_DB_ROOT="${SQL_TEST_DB_ROOT:-${ROOT_DIR}/tests/fixtures/data}"
PORT=18082
BIND_HOST=0.0.0.0
REQUEST_HOST=127.0.0.1
URL="http://${REQUEST_HOST}:${PORT}/query"
LOG_FILE="${ROOT_DIR}/.tmp/sql_api_server_error_cases.log"

mkdir -p "${ROOT_DIR}/.tmp"

cleanup() {
  if [[ -n "${SERVER_PID:-}" ]] && kill -0 "${SERVER_PID}" 2>/dev/null; then
    kill "${SERVER_PID}" 2>/dev/null || true
    wait "${SERVER_PID}" 2>/dev/null || true
  fi
}

assert_json_message() {
  local body_file="$1"
  local expected_status="$2"
  local expected_message="$3"
  local actual_status="$4"

  [[ "${actual_status}" == "${expected_status}" ]]
  BODY_FILE="${body_file}" EXPECTED_MESSAGE="${expected_message}" python3 - <<'PY'
import json
import os

body_file = os.environ["BODY_FILE"]
expected_message = os.environ["EXPECTED_MESSAGE"]

with open(body_file, "r", encoding="utf-8") as handle:
    payload = json.load(handle)

assert payload["ok"] is False
assert payload["error"]["message"] == expected_message
PY
}

trap cleanup EXIT

"${SERVER_BIN}" --host "${BIND_HOST}" --port "${PORT}" --db "${FIXTURE_DB_ROOT}" >"${LOG_FILE}" 2>&1 &
SERVER_PID=$!

for _ in $(seq 1 20); do
  if curl -sS -o /dev/null -X POST -H "Content-Type: text/plain" --data "SELECT * FROM users;" "${URL}" 2>/dev/null; then
    break
  fi
  sleep 0.2
done

status_code="$(curl -sS -o "${ROOT_DIR}/.tmp/api_get_error.json" -w "%{http_code}" "${URL}")"
assert_json_message "${ROOT_DIR}/.tmp/api_get_error.json" "405" "Only POST requests are supported." "${status_code}"

status_code="$(curl -sS -o "${ROOT_DIR}/.tmp/api_blank_body_error.json" -w "%{http_code}" \
  -X POST \
  -H "Content-Type: text/plain" \
  --data '' \
  "${URL}")"
assert_json_message "${ROOT_DIR}/.tmp/api_blank_body_error.json" "400" "Request body must contain SQL text." "${status_code}"

status_code="$(curl -sS -o "${ROOT_DIR}/.tmp/api_invalid_sql_error.json" -w "%{http_code}" \
  -X POST \
  -H "Content-Type: text/plain" \
  --data "DROP TABLE users;" \
  "${URL}")"
assert_json_message "${ROOT_DIR}/.tmp/api_invalid_sql_error.json" "400" "sql_service_classify_operation could not classify SQL" "${status_code}"

status_code="$(curl -sS -o "${ROOT_DIR}/.tmp/api_invalid_where_error.json" -w "%{http_code}" \
  -X POST \
  -H "Content-Type: text/plain" \
  --data "SELECT * FROM users WHERE id = 'abc';" \
  "${URL}")"
assert_json_message "${ROOT_DIR}/.tmp/api_invalid_where_error.json" "400" "WHERE id requires an integer value" "${status_code}"
