#include "executor.h"
#include "schema.h"
#include "storage.h"
#include <ctype.h>

static int is_integer_string(const char *text) {
    size_t index = 0;

    if (text == NULL) {
        return 0;
    }

    if (text[0] == '-' || text[0] == '+') {
        index = 1;
    }

    if (text[index] == '\0') {
        return 0;
    }

    for (; text[index] != '\0'; index++) {
        if (!isdigit((unsigned char) text[index])) {
            return 0;
        }
    }
    return 1;
}

static int is_float_string(const char *text) {
    size_t index = 0;
    int saw_digit = 0;
    int saw_dot = 0;

    if (text == NULL) {
        return 0;
    }

    if (text[0] == '-' || text[0] == '+') {
        index = 1;
    }

    for (; text[index] != '\0'; index++) {
        if (isdigit((unsigned char) text[index])) {
            saw_digit = 1;
            continue;
        }
        if (text[index] == '.' && !saw_dot) {
            saw_dot = 1;
            continue;
        }
        return 0;
    }

    return saw_digit;
}

static int validate_value(const ColumnDef *column, const char *value, SqlError *error) {
    if (column->type == COLUMN_TYPE_INT && !is_integer_string(value)) {
        sql_set_error(error, 0, 0, "column %s expects int", column->name);
        return 0;
    }

    if (column->type == COLUMN_TYPE_FLOAT && !is_float_string(value)) {
        sql_set_error(error, 0, 0, "column %s expects float", column->name);
        return 0;
    }

    return 1;
}

static int find_column_index(const Schema *schema, const char *column_name) {
    int index;

    for (index = 0; index < schema->column_count; index++) {
        if (strcmp(schema->columns[index].name, column_name) == 0) {
            return index;
        }
    }

    return -1;
}

static void print_header(const Schema *schema, FILE *out) {
    int index;

    for (index = 0; index < schema->column_count; index++) {
        if (index > 0) {
            fputc(',', out);
        }
        fputs(schema->columns[index].name, out);
    }
    fputc('\n', out);
}

static void print_row(const Row *row, FILE *out) {
    int index;

    for (index = 0; index < row->field_count; index++) {
        if (index > 0) {
            fputc(',', out);
        }
        fputs(row->fields[index], out);
    }
    fputc('\n', out);
}

static int execute_select(const Query *query, DbContext *ctx, FILE *out, SqlError *error) {
    TableState *table;
    int row_index;

    table = db_context_get_table(ctx, query->table_name, error);
    if (table == NULL) {
        return 0;
    }

    if (!query->has_condition) {
        print_header(&table->schema, out);
        for (row_index = 0; row_index < table->rowset.row_count; row_index++) {
            print_row(&table->rowset.rows[row_index], out);
        }
        return 1;
    }

    if (strcmp(query->condition.column, "id") == 0) {
        int matched_row_index = -1;
        if (!is_integer_string(query->condition.value.raw)) {
            sql_set_error(error, 0, 0, "WHERE id requires an integer value");
            return 0;
        }
        print_header(&table->schema, out);
        if (bptree_search(table->index, atoi(query->condition.value.raw), &matched_row_index)) {
            print_row(&table->rowset.rows[matched_row_index], out);
        }
        return 1;
    }

    {
        int column_index = find_column_index(&table->schema, query->condition.column);
        if (column_index < 0) {
            sql_set_error(error, 0, 0, "unknown column in WHERE clause: %s", query->condition.column);
            return 0;
        }

        print_header(&table->schema, out);
        for (row_index = 0; row_index < table->rowset.row_count; row_index++) {
            if (strcmp(table->rowset.rows[row_index].fields[column_index], query->condition.value.raw) == 0) {
                print_row(&table->rowset.rows[row_index], out);
            }
        }
    }

    return 1;
}

static int append_row_to_memory(TableState *table, char **fields, SqlError *error) {
    Row *next_rows;
    char **field_copies;
    int index;

    if (table->rowset.row_count == table->rowset.row_capacity) {
        int next_capacity = table->rowset.row_capacity == 0 ? 8 : table->rowset.row_capacity * 2;
        next_rows = (Row *) realloc(table->rowset.rows, (size_t) next_capacity * sizeof(Row));
        if (next_rows == NULL) {
            sql_set_error(error, 0, 0, "failed to expand in-memory rowset");
            return 0;
        }
        table->rowset.rows = next_rows;
        table->rowset.row_capacity = next_capacity;
    }

    field_copies = (char **) calloc((size_t) table->schema.column_count, sizeof(char *));
    if (field_copies == NULL) {
        sql_set_error(error, 0, 0, "failed to allocate in-memory row");
        return 0;
    }

    for (index = 0; index < table->schema.column_count; index++) {
        field_copies[index] = sql_strdup(fields[index]);
        if (field_copies[index] == NULL) {
            while (index > 0) {
                free(field_copies[--index]);
            }
            free(field_copies);
            sql_set_error(error, 0, 0, "failed to copy inserted row");
            return 0;
        }
    }

    table->rowset.rows[table->rowset.row_count].fields = field_copies;
    table->rowset.rows[table->rowset.row_count].field_count = table->schema.column_count;
    table->rowset.row_count++;
    return 1;
}

static int execute_insert(const Query *query, DbContext *ctx, FILE *out, SqlError *error) {
    TableState *table;
    char **fields = NULL;
    char id_buffer[32];
    int value_offset = 0;
    int inserted_row_index;
    int inserted_id = 0;
    int index;
    int ok = 0;

    table = db_context_get_table(ctx, query->table_name, error);
    if (table == NULL) {
        return 0;
    }

    if (query->value_count == table->schema.column_count) {
        value_offset = 0;
    } else if (table->schema.autoincrement && table->schema.primary_key_index == 0 &&
               query->value_count == table->schema.column_count - 1) {
        value_offset = 1;
    } else {
        sql_set_error(error, 0, 0, "VALUES count does not match schema");
        return 0;
    }

    fields = (char **) calloc((size_t) table->schema.column_count, sizeof(char *));
    if (fields == NULL) {
        sql_set_error(error, 0, 0, "failed to allocate insert row");
        return 0;
    }

    if (value_offset == 1) {
        snprintf(id_buffer, sizeof(id_buffer), "%d", table->next_id);
        fields[0] = sql_strdup(id_buffer);
        if (fields[0] == NULL) {
            sql_set_error(error, 0, 0, "failed to allocate id field");
            goto cleanup;
        }
    }

    for (index = value_offset; index < table->schema.column_count; index++) {
        fields[index] = sql_strdup(query->values[index - value_offset]);
        if (fields[index] == NULL) {
            sql_set_error(error, 0, 0, "failed to allocate field value");
            goto cleanup;
        }
    }

    for (index = 0; index < table->schema.column_count; index++) {
        if (!validate_value(&table->schema.columns[index], fields[index], error)) {
            goto cleanup;
        }
    }

    inserted_id = atoi(fields[table->schema.primary_key_index]);
    if (bptree_search(table->index, inserted_id, NULL)) {
        sql_set_error(error, 0, 0, "duplicate id: %d", inserted_id);
        goto cleanup;
    }

    if (!append_csv_row(ctx->db_root, query->table_name, fields, table->schema.column_count, error)) {
        goto cleanup;
    }

    inserted_row_index = table->rowset.row_count;
    if (!append_row_to_memory(table, fields, error)) {
        goto cleanup;
    }

    if (!bptree_insert(table->index, inserted_id, inserted_row_index, error)) {
        db_context_reload_table(ctx, query->table_name, error);
        goto cleanup;
    }

    if (inserted_id >= table->next_id) {
        table->next_id = inserted_id + 1;
    }

    fprintf(out, "INSERT 1\n");
    ok = 1;

cleanup:
    if (fields != NULL) {
        for (index = 0; index < table->schema.column_count; index++) {
            free(fields[index]);
        }
    }
    free(fields);
    return ok;
}

int execute_query(const Query *query, DbContext *ctx, FILE *out, SqlError *error) {
    if (query == NULL || ctx == NULL || out == NULL) {
        sql_set_error(error, 0, 0, "execute_query received invalid arguments");
        return 0;
    }

    if (query->type == QUERY_SELECT) {
        return execute_select(query, ctx, out, error);
    }

    if (query->type == QUERY_INSERT) {
        return execute_insert(query, ctx, out, error);
    }

    sql_set_error(error, 0, 0, "unsupported query type");
    return 0;
}
