#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SERVER_BIN="${ROOT_DIR}/sql_api_server"
PORT=18081
BIND_HOST=0.0.0.0
REQUEST_HOST=127.0.0.1
URL="http://${REQUEST_HOST}:${PORT}/query"
TMP_DIR="${ROOT_DIR}/.tmp/api-concurrency"
DB_ROOT="${TMP_DIR}/db"
LOG_FILE="${TMP_DIR}/sql_api_server.log"
SELECT_COUNT=8
INSERT_COUNT=10
select_job_pids=()
mixed_job_pids=()

mkdir -p "${TMP_DIR}" "${DB_ROOT}/schema" "${DB_ROOT}/tables"
cp "${ROOT_DIR}/data/schema/users.schema" "${DB_ROOT}/schema/users.schema"
cp "${ROOT_DIR}/data/tables/users.csv" "${DB_ROOT}/tables/users.csv"

cleanup() {
  if [[ -n "${SERVER_PID:-}" ]] && kill -0 "${SERVER_PID}" 2>/dev/null; then
    kill "${SERVER_PID}" 2>/dev/null || true
    wait "${SERVER_PID}" 2>/dev/null || true
  fi
}

wait_for_jobs() {
  local job_pid

  for job_pid in "$@"; do
    wait "${job_pid}"
  done
}

trap cleanup EXIT

"${SERVER_BIN}" --host "${BIND_HOST}" --port "${PORT}" --db "${DB_ROOT}" >"${LOG_FILE}" 2>&1 &
SERVER_PID=$!

for _ in $(seq 1 30); do
  if curl -sS -o /dev/null -X POST -H "Content-Type: text/plain" --data "SELECT * FROM users;" "${URL}" 2>/dev/null; then
    break
  fi
  sleep 0.2
done

run_select_request() {
  local index="$1"
  local body_file="${TMP_DIR}/select_${index}.json"
  local expected_body
  local actual_body
  local status

  status="$(curl -sS -o "${body_file}" -w "%{http_code}" \
    -X POST \
    -H "Content-Type: text/plain" \
    --data "SELECT * FROM users WHERE id = 2;" \
    "${URL}")"

  [[ "${status}" == "200" ]]
  expected_body="$(cat "${ROOT_DIR}/tests/integration/api_select.expected")"
  actual_body="$(cat "${body_file}")"
  [[ "${actual_body}" == "${expected_body}" ]]
}

run_insert_request() {
  local index="$1"
  local age=$((30 + index))
  local name
  local body_file="${TMP_DIR}/insert_${index}.json"
  local status

  name="$(printf "Parallel_%02d" "${index}")"
  status="$(curl -sS -o "${body_file}" -w "%{http_code}" \
    -X POST \
    -H "Content-Type: text/plain" \
    --data "INSERT INTO users VALUES ('${name}', ${age});" \
    "${URL}")"

  [[ "${status}" == "200" ]]
  BODY_FILE="${body_file}" EXPECTED_NAME="${name}" EXPECTED_AGE="${age}" python3 - <<'PY'
import json
import os

body_file = os.environ["BODY_FILE"]
expected_name = os.environ["EXPECTED_NAME"]
expected_age = int(os.environ["EXPECTED_AGE"])

with open(body_file, "r", encoding="utf-8") as handle:
    payload = json.load(handle)

assert payload["ok"] is True
assert payload["type"] == "insert"
assert payload["table"] == "users"
assert payload["affected_rows"] == 1
assert payload["message"] == "INSERT 1"
assert expected_name.startswith("Parallel_")
assert expected_age > 0
PY
}

for index in $(seq 1 "${SELECT_COUNT}"); do
  run_select_request "${index}" &
  select_job_pids[${#select_job_pids[@]}]=$!
done
wait_for_jobs "${select_job_pids[@]}"

for index in $(seq 1 "${INSERT_COUNT}"); do
  run_insert_request "${index}" &
  mixed_job_pids[${#mixed_job_pids[@]}]=$!
done

for index in $(seq 1 "${SELECT_COUNT}"); do
  run_select_request "mixed_${index}" &
  mixed_job_pids[${#mixed_job_pids[@]}]=$!
done
wait_for_jobs "${mixed_job_pids[@]}"

curl -sS -o "${TMP_DIR}/final_rows.json" \
  -X POST \
  -H "Content-Type: text/plain" \
  --data "SELECT * FROM users;" \
  "${URL}" >/dev/null

FINAL_BODY_FILE="${TMP_DIR}/final_rows.json" INSERT_COUNT="${INSERT_COUNT}" python3 - <<'PY'
import json
import os

body_file = os.environ["FINAL_BODY_FILE"]
insert_count = int(os.environ["INSERT_COUNT"])

with open(body_file, "r", encoding="utf-8") as handle:
    payload = json.load(handle)

assert payload["ok"] is True
assert payload["type"] == "select"
rows = payload["rows"]
assert len(rows) == 2 + insert_count

names = {row["name"] for row in rows}
expected_names = {f"Parallel_{index:02d}" for index in range(1, insert_count + 1)}
assert expected_names.issubset(names)
PY
