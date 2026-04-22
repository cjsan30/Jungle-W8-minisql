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
UNIT_CLI_TARGET := tests/unit/test_cli_parser
UNIT_BPTREE_TARGET := tests/unit/test_bptree
UNIT_SERVER_CONFIG_TARGET := tests/unit/test_server_config
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

.PHONY: all clean test unit integration benchmark-smoke smoke install-hooks verify-hooks ci

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $(SRCS)

$(UNIT_CLI_TARGET): tests/unit/test_cli_parser.c src/runtime/cli.c src/query/tokenizer.c src/query/parser.c src/common.c
	$(CC) $(CFLAGS) -o $@ tests/unit/test_cli_parser.c src/runtime/cli.c src/query/tokenizer.c src/query/parser.c src/common.c

$(UNIT_BPTREE_TARGET): tests/unit/test_bptree.c src/index/bptree.c src/common.c
	$(CC) $(CFLAGS) -o $@ tests/unit/test_bptree.c src/index/bptree.c src/common.c

$(UNIT_SERVER_CONFIG_TARGET): tests/unit/test_server_config.c src/server/api/server_config.c src/common.c
	$(CC) $(CFLAGS) -o $@ tests/unit/test_server_config.c src/server/api/server_config.c src/common.c

unit: $(UNIT_CLI_TARGET) $(UNIT_BPTREE_TARGET) $(UNIT_SERVER_CONFIG_TARGET)
	@./$(UNIT_CLI_TARGET)
	@./$(UNIT_BPTREE_TARGET)
	@./$(UNIT_SERVER_CONFIG_TARGET)

integration: $(TARGET)
	@$(ASSERT_CLI) -Executable ./$(TARGET) -SqlFile $(SMOKE_SQL) -ExpectedFile $(SMOKE_EXPECTED)
	@$(ASSERT_CLI) -Executable ./$(TARGET) -SqlFile $(WHERE_SQL) -ExpectedFile $(WHERE_EXPECTED)
	@$(ASSERT_CLI) -Executable ./$(TARGET) -SqlFile $(WHERE_NAME_SQL) -ExpectedFile $(WHERE_NAME_EXPECTED)
	@bash tests/integration/assert_insert_roundtrip.sh
	@bash tests/integration/assert_csv_quoted_roundtrip.sh
	@bash tests/integration/assert_multi_statement_roundtrip.sh
	@bash tests/integration/assert_commented_script_roundtrip.sh
	@bash tests/integration/assert_duplicate_id_rejected.sh
	@bash tests/integration/assert_invalid_persisted_row_rejected.sh

integration-invalid: $(TARGET)
	@$(ASSERT_CLI) -Executable ./$(TARGET) -SqlFile $(INVALID_SQL) -ExpectFailure -ExpectedPattern error:

benchmark-smoke: $(TARGET)
ifeq ($(UNAME_S),Linux)
	@bash tests/integration/assert_benchmark_output.sh
else
	@$(POWERSHELL) -NoProfile -File tests/integration/assert_benchmark_output.ps1 -Executable ./$(TARGET) -DbRoot ./data
endif

verify-hooks:
	@bash tests/integration/assert_git_hooks_installed.sh

smoke: integration benchmark-smoke

test: unit integration integration-invalid benchmark-smoke verify-hooks

ci: test

clean:
ifeq ($(UNAME_S),Linux)
	-@rm -f $(TARGET) $(UNIT_CLI_TARGET) $(UNIT_BPTREE_TARGET) $(UNIT_SERVER_CONFIG_TARGET)
else
	-@$(POWERSHELL) -NoProfile -Command "Remove-Item -Force -ErrorAction SilentlyContinue '$(TARGET)', '$(UNIT_CLI_TARGET)', '$(UNIT_BPTREE_TARGET)', '$(UNIT_SERVER_CONFIG_TARGET)'"
endif

install-hooks:
	@bash scripts/install_git_hooks.sh
