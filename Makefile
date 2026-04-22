CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude
POWERSHELL ?= powershell.exe
UNAME_S := $(shell uname -s 2>/dev/null)

ifeq ($(UNAME_S),Linux)
ASSERT_CLI = bash tests/integration/assert_cli_output.sh
else
ASSERT_CLI = $(POWERSHELL) -NoProfile -File tests/integration/assert_cli_output.ps1
endif

TARGET := sql_processor
SERVER_TARGET := sql_api_server
UNIT_CLI_TARGET := tests/unit/test_cli_parser
UNIT_BPTREE_TARGET := tests/unit/test_bptree
UNIT_SERVER_CONFIG_TARGET := tests/unit/test_server_config
UNIT_HTTP_PROTOCOL_TARGET := tests/unit/test_http_protocol
UNIT_SQL_SERVICE_TARGET := tests/unit/test_sql_service
UNIT_REQUEST_HANDLER_TARGET := tests/unit/test_request_handler
UNIT_DB_GUARD_TARGET := tests/unit/test_db_guard
SMOKE_SQL := tests/integration/smoke.sql
SMOKE_EXPECTED := tests/integration/smoke.expected
WHERE_SQL := tests/integration/where_id.sql
WHERE_EXPECTED := tests/integration/where_id.expected
WHERE_NAME_SQL := tests/integration/where_name.sql
WHERE_NAME_EXPECTED := tests/integration/where_name.expected
BENCH_EXPECTED := tests/integration/benchmark_smoke.expected
INVALID_SQL := tests/integration/empty.sql
SRCS := \
	src/runtime/main.c \
	src/runtime/cli.c \
	src/query/tokenizer.c \
	src/query/parser.c \
	src/query/executor.c \
	src/storage/schema.c \
	src/storage/storage.c \
	src/index/bptree.c \
	src/index/db_context.c \
	src/common.c
SERVER_SRCS := \
	src/server/api/server_runtime_main.c \
	src/server/api/server_main.c \
	src/server/api/socket_server.c \
	src/server/api/server_config.c \
	src/server/pool/thread_pool.c \
	src/server/pool/job_queue.c \
	src/server/pool/server_job_stub.c \
	src/server/request/http_protocol.c \
	src/server/request/request_handler.c \
	src/server/request/sql_service.c \
	src/server/concurrency/db_guard.c \
	src/query/tokenizer.c \
	src/query/parser.c \
	src/query/executor.c \
	src/storage/schema.c \
	src/storage/storage.c \
	src/index/bptree.c \
	src/index/db_context.c \
	src/common.c
SERVER_LDFLAGS ?= -pthread

.PHONY: all clean test unit integration benchmark-smoke smoke install-hooks verify-hooks api-smoke ci

all: $(TARGET) $(SERVER_TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $(SRCS)

$(SERVER_TARGET): $(SERVER_SRCS)
	$(CC) $(CFLAGS) $(SERVER_LDFLAGS) -o $@ $(SERVER_SRCS) $(SERVER_LDFLAGS)

$(UNIT_CLI_TARGET): tests/unit/test_cli_parser.c src/runtime/cli.c src/query/tokenizer.c src/query/parser.c src/common.c
	$(CC) $(CFLAGS) -o $@ tests/unit/test_cli_parser.c src/runtime/cli.c src/query/tokenizer.c src/query/parser.c src/common.c

$(UNIT_BPTREE_TARGET): tests/unit/test_bptree.c src/index/bptree.c src/common.c
	$(CC) $(CFLAGS) -o $@ tests/unit/test_bptree.c src/index/bptree.c src/common.c

$(UNIT_SERVER_CONFIG_TARGET): tests/unit/test_server_config.c src/server/api/server_config.c src/common.c
	$(CC) $(CFLAGS) -o $@ tests/unit/test_server_config.c src/server/api/server_config.c src/common.c

$(UNIT_HTTP_PROTOCOL_TARGET): tests/unit/test_http_protocol.c src/server/request/http_protocol.c src/common.c
	$(CC) $(CFLAGS) -o $@ tests/unit/test_http_protocol.c src/server/request/http_protocol.c src/common.c

$(UNIT_SQL_SERVICE_TARGET): tests/unit/test_sql_service.c src/server/request/sql_service.c src/query/tokenizer.c src/query/parser.c src/query/executor.c src/storage/schema.c src/storage/storage.c src/index/bptree.c src/index/db_context.c src/common.c
	$(CC) $(CFLAGS) -o $@ tests/unit/test_sql_service.c src/server/request/sql_service.c src/query/tokenizer.c src/query/parser.c src/query/executor.c src/storage/schema.c src/storage/storage.c src/index/bptree.c src/index/db_context.c src/common.c

$(UNIT_REQUEST_HANDLER_TARGET): tests/unit/test_request_handler.c src/server/request/request_handler.c src/server/request/http_protocol.c src/server/request/sql_service.c src/server/concurrency/db_guard.c src/query/tokenizer.c src/query/parser.c src/query/executor.c src/storage/schema.c src/storage/storage.c src/index/bptree.c src/index/db_context.c src/common.c
	$(CC) $(CFLAGS) -o $@ tests/unit/test_request_handler.c src/server/request/request_handler.c src/server/request/http_protocol.c src/server/request/sql_service.c src/server/concurrency/db_guard.c src/query/tokenizer.c src/query/parser.c src/query/executor.c src/storage/schema.c src/storage/storage.c src/index/bptree.c src/index/db_context.c src/common.c

$(UNIT_DB_GUARD_TARGET): tests/unit/test_db_guard.c src/server/concurrency/db_guard.c src/common.c
	$(CC) $(CFLAGS) $(SERVER_LDFLAGS) -o $@ tests/unit/test_db_guard.c src/server/concurrency/db_guard.c src/common.c $(SERVER_LDFLAGS)

unit: $(UNIT_CLI_TARGET) $(UNIT_BPTREE_TARGET) $(UNIT_SERVER_CONFIG_TARGET) $(UNIT_HTTP_PROTOCOL_TARGET) $(UNIT_SQL_SERVICE_TARGET) $(UNIT_REQUEST_HANDLER_TARGET) $(UNIT_DB_GUARD_TARGET)
	@./$(UNIT_CLI_TARGET)
	@./$(UNIT_BPTREE_TARGET)
	@./$(UNIT_SERVER_CONFIG_TARGET)
	@./$(UNIT_HTTP_PROTOCOL_TARGET)
	@./$(UNIT_SQL_SERVICE_TARGET)
	@./$(UNIT_REQUEST_HANDLER_TARGET)
	@./$(UNIT_DB_GUARD_TARGET)

integration: $(TARGET)
ifeq ($(UNAME_S),Linux)
	@$(ASSERT_CLI) -Executable ./$(TARGET) -SqlFile $(SMOKE_SQL) -ExpectedFile $(SMOKE_EXPECTED)
	@$(ASSERT_CLI) -Executable ./$(TARGET) -SqlFile $(WHERE_SQL) -ExpectedFile $(WHERE_EXPECTED)
	@$(ASSERT_CLI) -Executable ./$(TARGET) -SqlFile $(WHERE_NAME_SQL) -ExpectedFile $(WHERE_NAME_EXPECTED)
	@bash tests/integration/assert_insert_roundtrip.sh
	@bash tests/integration/assert_csv_quoted_roundtrip.sh
	@bash tests/integration/assert_multi_statement_roundtrip.sh
	@bash tests/integration/assert_commented_script_roundtrip.sh
	@bash tests/integration/assert_duplicate_id_rejected.sh
	@bash tests/integration/assert_invalid_persisted_row_rejected.sh
else
	@$(POWERSHELL) -NoProfile -File tests/integration/run_cli_integration.ps1 -Executable ./$(TARGET) -DbRoot ./data
endif

integration-invalid: $(TARGET)
	@$(ASSERT_CLI) -Executable ./$(TARGET) -SqlFile $(INVALID_SQL) -ExpectFailure -ExpectedPattern error:

benchmark-smoke: $(TARGET)
ifeq ($(UNAME_S),Linux)
	@bash tests/integration/assert_benchmark_output.sh
else
	@$(POWERSHELL) -NoProfile -File tests/integration/assert_benchmark_output.ps1 -Executable ./$(TARGET) -DbRoot ./data
endif

verify-hooks:
ifeq ($(UNAME_S),Linux)
	@bash tests/integration/assert_git_hooks_installed.sh
else
	@$(POWERSHELL) -NoProfile -File tests/integration/assert_git_hooks_installed.ps1
endif

api-smoke: $(SERVER_TARGET)
	@bash tests/integration/assert_api_select.sh
	@bash tests/integration/assert_api_error_cases.sh
	@bash tests/integration/assert_api_concurrency.sh

smoke: integration benchmark-smoke

test: unit integration integration-invalid benchmark-smoke verify-hooks

ci: test

clean:
ifeq ($(UNAME_S),Linux)
	-@rm -f $(TARGET) $(SERVER_TARGET) $(UNIT_CLI_TARGET) $(UNIT_BPTREE_TARGET) $(UNIT_SERVER_CONFIG_TARGET) $(UNIT_HTTP_PROTOCOL_TARGET) $(UNIT_SQL_SERVICE_TARGET) $(UNIT_REQUEST_HANDLER_TARGET) $(UNIT_DB_GUARD_TARGET)
else
	-@$(POWERSHELL) -NoProfile -Command "Remove-Item -Force -ErrorAction SilentlyContinue '$(TARGET)', '$(SERVER_TARGET)', '$(UNIT_CLI_TARGET)', '$(UNIT_BPTREE_TARGET)', '$(UNIT_SERVER_CONFIG_TARGET)', '$(UNIT_HTTP_PROTOCOL_TARGET)', '$(UNIT_SQL_SERVICE_TARGET)', '$(UNIT_REQUEST_HANDLER_TARGET)', '$(UNIT_DB_GUARD_TARGET)'"
endif

install-hooks:
	@bash scripts/install_git_hooks.sh
