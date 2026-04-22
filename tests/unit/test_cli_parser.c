#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "cli.h"
#include "common.h"
#include "parser.h"

static void test_parse_cli_args_rejects_missing_required_input(void) {
    CliOptions options = {0};
    SqlError error = {0};
    char *argv[] = {"sql_processor"};

    assert(parse_cli_args(1, argv, &options, &error) == 0);
    assert(strstr(error.message, "either --sql, --query, or --bench") != NULL);
}

static void test_parse_cli_args_accepts_sql_path(void) {
    CliOptions options = {0};
    SqlError error = {0};
    char *argv[] = {"sql_processor", "--sql", "tests/integration/smoke.sql", "--db", "./data"};

    assert(parse_cli_args(5, argv, &options, &error) == 1);
    assert(strcmp(options.sql_path, "tests/integration/smoke.sql") == 0);
    assert(strcmp(options.db_root, "./data") == 0);
}

static void test_parse_cli_args_accepts_bench_mode(void) {
    CliOptions options = {0};
    SqlError error = {0};
    char *argv[] = {"sql_processor", "--bench", "10"};

    assert(parse_cli_args(3, argv, &options, &error) == 1);
    assert(options.bench_rows == 10L);
}

static void test_parse_cli_args_accepts_query_and_param(void) {
    CliOptions options = {0};
    SqlError error = {0};
    char *argv[] = {"sql_processor", "--query", "SELECT * FROM users WHERE id = ?;", "--param", "2"};

    assert(parse_cli_args(5, argv, &options, &error) == 1);
    assert(strcmp(options.query_text, "SELECT * FROM users WHERE id = ?;") == 0);
    assert(strcmp(options.param_value, "2") == 0);
}

static void test_parse_cli_args_rejects_sql_and_query_together(void) {
    CliOptions options = {0};
    SqlError error = {0};
    char *argv[] = {"sql_processor", "--sql", "a.sql", "--query", "SELECT * FROM users;"};

    assert(parse_cli_args(5, argv, &options, &error) == 0);
    assert(strstr(error.message, "--sql and --query cannot be used together") != NULL);
}

static void test_sql_read_text_file_reads_smoke_sql(void) {
    SqlError error = {0};
    char *sql_text = sql_read_text_file("tests/integration/smoke.sql", &error);

    assert(sql_text != NULL);
    assert(strstr(sql_text, "SELECT * FROM users;") != NULL);
    free(sql_text);
}

static void test_parse_sql_text_rejects_empty_sql(void) {
    SqlError error = {0};
    Query query = {0};

    assert(parse_sql_text("", &query, &error) == 0);
    assert(strstr(error.message, "sql must not be empty") != NULL);
}

static void test_parse_sql_text_rejects_whitespace_sql(void) {
    SqlError error = {0};
    Query query = {0};

    assert(parse_sql_text("   \n\t", &query, &error) == 0);
    assert(strstr(error.message, "sql must not be empty") != NULL);
}

static void test_parse_sql_text_accepts_smoke_sql(void) {
    SqlError error = {0};
    Query query = {0};

    assert(parse_sql_text("SELECT * FROM users;", &query, &error) == 1);
    assert(query.type == QUERY_SELECT);
    assert(strcmp(query.table_name, "users") == 0);
    free(query.table_name);
}

static void test_parse_sql_text_accepts_where_clause(void) {
    SqlError error = {0};
    Query query = {0};

    assert(parse_sql_text("SELECT * FROM users WHERE id = 2;", &query, &error) == 1);
    assert(query.type == QUERY_SELECT);
    assert(query.has_condition == 1);
    assert(strcmp(query.condition.column, "id") == 0);
    assert(strcmp(query.condition.value.raw, "2") == 0);
    free(query.table_name);
    free(query.condition.column);
    free(query.condition.value.raw);
}

static void test_parse_sql_text_accepts_insert_values(void) {
    SqlError error = {0};
    Query query = {0};

    assert(parse_sql_text("INSERT INTO users VALUES ('Carol', 25);", &query, &error) == 1);
    assert(query.type == QUERY_INSERT);
    assert(strcmp(query.table_name, "users") == 0);
    assert(query.value_count == 2);
    assert(strcmp(query.values[0], "Carol") == 0);
    assert(strcmp(query.values[1], "25") == 0);
    free(query.table_name);
    free(query.values[0]);
    free(query.values[1]);
    free(query.values);
}

int main(void) {
    test_parse_cli_args_rejects_missing_required_input();
    test_parse_cli_args_accepts_sql_path();
    test_parse_cli_args_accepts_bench_mode();
    test_parse_cli_args_accepts_query_and_param();
    test_parse_cli_args_rejects_sql_and_query_together();
    test_sql_read_text_file_reads_smoke_sql();
    test_parse_sql_text_rejects_empty_sql();
    test_parse_sql_text_rejects_whitespace_sql();
    test_parse_sql_text_accepts_smoke_sql();
    test_parse_sql_text_accepts_where_clause();
    test_parse_sql_text_accepts_insert_values();
    printf("cli/parser unit tests passed\n");
    return 0;
}
