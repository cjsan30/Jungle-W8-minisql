#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SERVER_BIN="${ROOT_DIR}/sql_api_server"
PORT=18080
BIND_HOST=0.0.0.0
REQUEST_HOST=127.0.0.1
URL="http://${REQUEST_HOST}:${PORT}/query"
LOG_FILE="${ROOT_DIR}/.tmp/sql_api_server.log"
HEADERS_FILE="${ROOT_DIR}/.tmp/api_select_headers.txt"
BODY_FILE="${ROOT_DIR}/.tmp/api_select_body.txt"

mkdir -p "${ROOT_DIR}/.tmp"

cleanup() {
  if [[ -n "${SERVER_PID:-}" ]] && kill -0 "${SERVER_PID}" 2>/dev/null; then
    kill "${SERVER_PID}" 2>/dev/null || true
    wait "${SERVER_PID}" 2>/dev/null || true
  fi
}

trap cleanup EXIT

"${SERVER_BIN}" --host "${BIND_HOST}" --port "${PORT}" --db "${ROOT_DIR}/data" >"${LOG_FILE}" 2>&1 &
SERVER_PID=$!

for _ in $(seq 1 20); do
  if curl -sS -o /dev/null "${URL}" 2>/dev/null; then
    break
  fi
  sleep 0.2
done

status_code="$(curl -sS -D "${HEADERS_FILE}" -o "${BODY_FILE}" -w "%{http_code}" \
  -X POST \
  -H "Content-Type: text/plain" \
  --data "SELECT * FROM users WHERE id = 2;" \
  "${URL}")"

if [[ "${status_code}" != "200" ]]; then
  echo "expected 200 but received ${status_code}" >&2
  cat "${LOG_FILE}" >&2 || true
  exit 1
fi

grep -iq '^Content-Type: application/json; charset=utf-8' "${HEADERS_FILE}"

expected_body="$(cat "${ROOT_DIR}/tests/integration/api_select.expected")"
actual_body="$(cat "${BODY_FILE}")"

if [[ "${actual_body}" != "${expected_body}" ]]; then
  diff -u "${ROOT_DIR}/tests/integration/api_select.expected" "${BODY_FILE}"
  exit 1
fi
