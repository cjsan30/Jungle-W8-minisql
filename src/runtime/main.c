#include "cli.h"
#include "common.h"
#include "db_context.h"
#include "executor.h"
#include "parser.h"
#include <ctype.h>
#include <time.h>

static void free_query(Query *query) {
    int index;

    if (query == NULL) {
        return;
    }

    free(query->table_name);
    query->table_name = NULL;

    for (index = 0; index < query->value_count; index++) {
        free(query->values[index]);
    }
    free(query->values);
    query->values = NULL;
    query->value_count = 0;

    free(query->condition.column);
    query->condition.column = NULL;
    free(query->condition.value.raw);
    query->condition.value.raw = NULL;
    query->has_condition = 0;
}

static int run_benchmark(DbContext *ctx, long iterations, FILE *out, SqlError *error) {
    TableState *table;
    int index_hits = 0;
    int linear_hits = 0;
    long iteration;
    int probe_stride;
    int distinct_probes;
    clock_t load_begin;
    clock_t load_end;
    clock_t index_begin;
    clock_t index_end;
    clock_t linear_begin;
    clock_t linear_end;
    double load_build_ms;
    double index_ms;
    double linear_ms;
    double speedup_ratio = 0.0;
    long linear_steps = 0;
    double average_linear_steps = 0.0;
    int max_linear_steps = 0;

    load_begin = clock();
    table = db_context_get_table(ctx, "users", error);
    load_end = clock();
    if (table == NULL) {
        return 0;
    }

    load_build_ms = ((double) (load_end - load_begin) * 1000.0) / (double) CLOCKS_PER_SEC;

    if (table->rowset.row_count == 0) {
        fprintf(out, "benchmark rows=0 iterations=%ld\n", iterations);
        fprintf(out, "load_build_ms=%.3f\n", load_build_ms);
        fprintf(out, "index_hits=0\n");
        fprintf(out, "linear_hits=0\n");
        fprintf(out, "distinct_probes=0\n");
        fprintf(out, "avg_linear_steps=0.000\n");
        fprintf(out, "max_linear_steps=0\n");
        fprintf(out, "index_ms=0.000\n");
        fprintf(out, "linear_ms=0.000\n");
        fprintf(out, "speedup_ratio=0.000\n");
        return 1;
    }

    probe_stride = table->rowset.row_count > 1 ? table->rowset.row_count - 1 : 1;
    distinct_probes = (int) (iterations < table->rowset.row_count ? iterations : table->rowset.row_count);

    for (iteration = 0; iteration < iterations && iteration < table->rowset.row_count; iteration++) {
        int probe_row = (int) ((iteration * (long) probe_stride) % table->rowset.row_count);
        int probe_id = atoi(table->rowset.rows[probe_row].fields[table->schema.primary_key_index]);
        const char *probe_name = table->rowset.rows[probe_row].fields[1];
        int row_index = -1;
        int scan_index;

        (void) bptree_search(table->index, probe_id, &row_index);
        for (scan_index = 0; scan_index < table->rowset.row_count; scan_index++) {
            if (strcmp(table->rowset.rows[scan_index].fields[1], probe_name) == 0) {
                break;
            }
        }
    }

    index_begin = clock();
    for (iteration = 0; iteration < iterations; iteration++) {
        int probe_row = (int) ((iteration * (long) probe_stride) % table->rowset.row_count);
        int probe_id = atoi(table->rowset.rows[probe_row].fields[table->schema.primary_key_index]);
        int row_index = -1;
        if (bptree_search(table->index, probe_id, &row_index) && row_index >= 0) {
            index_hits++;
        }
    }
    index_end = clock();

    linear_begin = clock();
    for (iteration = 0; iteration < iterations; iteration++) {
        int probe_row = (int) ((iteration * (long) probe_stride) % table->rowset.row_count);
        const char *probe_name = table->rowset.rows[probe_row].fields[1];
        int scan_index;

        for (scan_index = 0; scan_index < table->rowset.row_count; scan_index++) {
            linear_steps++;
            if (strcmp(table->rowset.rows[scan_index].fields[1], probe_name) == 0) {
                if (scan_index + 1 > max_linear_steps) {
                    max_linear_steps = scan_index + 1;
                }
                linear_hits++;
                break;
            }
        }
    }
    linear_end = clock();

    index_ms = ((double) (index_end - index_begin) * 1000.0) / (double) CLOCKS_PER_SEC;
    linear_ms = ((double) (linear_end - linear_begin) * 1000.0) / (double) CLOCKS_PER_SEC;
    average_linear_steps = iterations > 0 ? (double) linear_steps / (double) iterations : 0.0;
    if (index_ms > 0.0) {
        speedup_ratio = linear_ms / index_ms;
    }

    fprintf(out, "benchmark rows=%d iterations=%ld\n", table->rowset.row_count, iterations);
    fprintf(out, "load_build_ms=%.3f\n", load_build_ms);
    fprintf(out, "index_hits=%d\n", index_hits);
    fprintf(out, "linear_hits=%d\n", linear_hits);
    fprintf(out, "distinct_probes=%d\n", distinct_probes);
    fprintf(out, "avg_linear_steps=%.3f\n", average_linear_steps);
    fprintf(out, "max_linear_steps=%d\n", max_linear_steps);
    fprintf(out, "index_ms=%.3f\n", index_ms);
    fprintf(out, "linear_ms=%.3f\n", linear_ms);
    fprintf(out, "speedup_ratio=%.3f\n", speedup_ratio);
    return 1;
}

static int apply_single_param(char **sql_text, const char *param_value, SqlError *error) {
    char *source;
    char *cursor;
    char quote = '\0';
    char *placeholder = NULL;
    size_t prefix_length;
    size_t param_length;
    size_t suffix_length;
    char *resolved;

    if (sql_text == NULL || *sql_text == NULL || param_value == NULL) {
        sql_set_error(error, 0, 0, "apply_single_param received invalid arguments");
        return 0;
    }

    source = *sql_text;
    for (cursor = source; *cursor != '\0'; cursor++) {
        if (quote != '\0') {
            if (*cursor == quote) {
                quote = '\0';
            }
            continue;
        }

        if (*cursor == '\'' || *cursor == '"') {
            quote = *cursor;
            continue;
        }

        if (*cursor == '?') {
            placeholder = cursor;
            break;
        }
    }

    if (placeholder == NULL) {
        sql_set_error(error, 0, 0, "--param was provided but no '?' placeholder was found");
        return 0;
    }

    prefix_length = (size_t) (placeholder - source);
    param_length = strlen(param_value);
    suffix_length = strlen(placeholder + 1);
    resolved = (char *) malloc(prefix_length + param_length + suffix_length + 1U);
    if (resolved == NULL) {
        sql_set_error(error, 0, 0, "failed to allocate resolved query");
        return 0;
    }

    memcpy(resolved, source, prefix_length);
    memcpy(resolved + prefix_length, param_value, param_length);
    memcpy(resolved + prefix_length + param_length, placeholder + 1, suffix_length + 1U);
    free(source);
    *sql_text = resolved;
    return 1;
}

static char *trim_statement(char *text) {
    char *start = text;
    char *end;

    while (*start != '\0' && isspace((unsigned char) *start)) {
        start++;
    }

    end = start + strlen(start);
    while (end > start && isspace((unsigned char) end[-1])) {
        end--;
    }
    *end = '\0';
    return start;
}

static void strip_line_comments(char *sql_text) {
    char quote = '\0';
    char *cursor;

    for (cursor = sql_text; *cursor != '\0'; cursor++) {
        if (quote != '\0') {
            if (*cursor == quote) {
                quote = '\0';
            }
            continue;
        }

        if (*cursor == '\'' || *cursor == '"') {
            quote = *cursor;
            continue;
        }

        if (cursor[0] == '-' && cursor[1] == '-') {
            while (*cursor != '\0' && *cursor != '\n') {
                *cursor = ' ';
                cursor++;
            }
            if (*cursor == '\0') {
                break;
            }
        }
    }
}

static int execute_sql_script(char *sql_text, DbContext *ctx, FILE *out, SqlError *error) {
    char *statement_start = sql_text;
    char quote = '\0';
    char *cursor;
    int executed_any = 0;

    strip_line_comments(sql_text);

    for (cursor = sql_text; ; cursor++) {
        int at_end = (*cursor == '\0');
        int statement_break = 0;

        if (!at_end) {
            if (quote != '\0') {
                if (*cursor == quote) {
                    quote = '\0';
                }
            } else if (*cursor == '\'' || *cursor == '"') {
                quote = *cursor;
            } else if (*cursor == ';') {
                statement_break = 1;
            }
        }

        if (statement_break || at_end) {
            Query query = {0};
            char saved = *cursor;
            char *statement;

            *cursor = '\0';
            statement = trim_statement(statement_start);
            if (*statement != '\0') {
                executed_any = 1;
                if (!parse_sql_text(statement, &query, error)) {
                    free_query(&query);
                    *cursor = saved;
                    return 0;
                }
                if (!execute_query(&query, ctx, out, error)) {
                    free_query(&query);
                    *cursor = saved;
                    return 0;
                }
                free_query(&query);
            }

            *cursor = saved;
            if (at_end) {
                break;
            }
            statement_start = cursor + 1;
        }
    }

    if (!executed_any) {
        sql_set_error(error, 0, 0, "sql must not be empty");
        return 0;
    }

    return 1;
}

int main(int argc, char **argv) {
    CliOptions options = {0};
    SqlError error = {0};
    DbContext *ctx = NULL;
    char *sql_text = NULL;

    if (!parse_cli_args(argc, argv, &options, &error)) {
        print_usage(stderr, argv[0]);
        fprintf(stderr, "error: %s\n", error.message);
        return 1;
    }

    ctx = db_context_create(options.db_root, &error);
    if (ctx == NULL) {
        fprintf(stderr, "error: %s\n", error.message);
        return 1;
    }

    if (options.bench_rows > 0) {
        if (!run_benchmark(ctx, options.bench_rows, stdout, &error)) {
            fprintf(stderr, "error: %s\n", error.message);
            db_context_destroy(ctx);
            return 1;
        }
        db_context_destroy(ctx);
        return 0;
    }

    if (options.query_text != NULL) {
        sql_text = sql_strdup(options.query_text);
        if (sql_text == NULL) {
            fprintf(stderr, "error: failed to allocate query text\n");
            db_context_destroy(ctx);
            return 1;
        }
    } else {
        sql_text = sql_read_text_file(options.sql_path, &error);
        if (sql_text == NULL) {
            fprintf(stderr, "error: %s\n", error.message);
            db_context_destroy(ctx);
            return 1;
        }
    }

    if (options.param_value != NULL && !apply_single_param(&sql_text, options.param_value, &error)) {
        fprintf(stderr, "error: %s\n", error.message);
        free(sql_text);
        db_context_destroy(ctx);
        return 1;
    }

    if (!execute_sql_script(sql_text, ctx, stdout, &error)) {
        fprintf(stderr, "error: %s\n", error.message);
        free(sql_text);
        db_context_destroy(ctx);
        return 1;
    }

    free(sql_text);
    db_context_destroy(ctx);
    return 0;
}
