#include "db_context.h"
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

static int validate_loaded_value(const ColumnDef *column, const char *value, SqlError *error) {
    if (column->type == COLUMN_TYPE_INT && !is_integer_string(value)) {
        sql_set_error(error, 0, 0, "invalid persisted int for column %s: %s", column->name, value);
        return 0;
    }

    if (column->type == COLUMN_TYPE_FLOAT && !is_float_string(value)) {
        sql_set_error(error, 0, 0, "invalid persisted float for column %s: %s", column->name, value);
        return 0;
    }

    return 1;
}

static int validate_loaded_rowset(const Schema *schema, const RowSet *rowset, SqlError *error) {
    int row_index;
    int column_index;

    for (row_index = 0; row_index < rowset->row_count; row_index++) {
        if (rowset->rows[row_index].field_count != schema->column_count) {
            sql_set_error(error, 0, 0, "persisted row field count does not match schema");
            return 0;
        }

        for (column_index = 0; column_index < schema->column_count; column_index++) {
            if (!validate_loaded_value(&schema->columns[column_index],
                                       rowset->rows[row_index].fields[column_index], error)) {
                return 0;
            }
        }
    }

    return 1;
}

static void table_state_destroy(TableState *table) {
    if (table == NULL) {
        return;
    }

    free_schema(&table->schema);
    free_rowset(&table->rowset);
    bptree_destroy(table->index);
    table->index = NULL;
    table->next_id = 0;
}

static int table_state_load(TableState *table, const char *db_root, const char *table_name, SqlError *error) {
    int row_index;
    int max_id = 0;

    memset(table, 0, sizeof(*table));
    strncpy(table->name, table_name, sizeof(table->name) - 1);

    if (!load_schema(db_root, table_name, &table->schema, error)) {
        table_state_destroy(table);
        return 0;
    }

    if (!read_csv_rows(db_root, table_name, table->schema.column_count, &table->rowset, error)) {
        table_state_destroy(table);
        return 0;
    }

    if (!validate_loaded_rowset(&table->schema, &table->rowset, error)) {
        table_state_destroy(table);
        return 0;
    }

    if (table->schema.primary_key_index < 0) {
        table_state_destroy(table);
        sql_set_error(error, 0, 0, "table %s is missing a valid primary key", table_name);
        return 0;
    }

    table->index = bptree_create();
    if (table->index == NULL) {
        table_state_destroy(table);
        sql_set_error(error, 0, 0, "failed to allocate B+ tree");
        return 0;
    }

    for (row_index = 0; row_index < table->rowset.row_count; row_index++) {
        int id = atoi(table->rowset.rows[row_index].fields[table->schema.primary_key_index]);
        if (!bptree_insert(table->index, id, row_index, error)) {
            table_state_destroy(table);
            return 0;
        }
        if (id > max_id) {
            max_id = id;
        }
    }

    table->next_id = max_id + 1;
    if (table->next_id <= 0) {
        table->next_id = 1;
    }

    return 1;
}

DbContext *db_context_create(const char *db_root, SqlError *error) {
    DbContext *ctx;

    if (db_root == NULL) {
        sql_set_error(error, 0, 0, "db_root must not be null");
        return NULL;
    }

    ctx = (DbContext *) calloc(1, sizeof(DbContext));
    if (ctx == NULL) {
        sql_set_error(error, 0, 0, "failed to allocate db context");
        return NULL;
    }

    strncpy(ctx->db_root, db_root, sizeof(ctx->db_root) - 1);
    return ctx;
}

void db_context_destroy(DbContext *ctx) {
    int index;

    if (ctx == NULL) {
        return;
    }

    for (index = 0; index < ctx->table_count; index++) {
        table_state_destroy(&ctx->tables[index]);
    }
    free(ctx->tables);
    free(ctx);
}

TableState *db_context_get_table(DbContext *ctx, const char *table_name, SqlError *error) {
    TableState *next_tables;
    int index;

    if (ctx == NULL || table_name == NULL) {
        sql_set_error(error, 0, 0, "db_context_get_table received invalid arguments");
        return NULL;
    }

    for (index = 0; index < ctx->table_count; index++) {
        if (strcmp(ctx->tables[index].name, table_name) == 0) {
            return &ctx->tables[index];
        }
    }

    next_tables = (TableState *) realloc(ctx->tables, (size_t) (ctx->table_count + 1) * sizeof(TableState));
    if (next_tables == NULL) {
        sql_set_error(error, 0, 0, "failed to expand db context tables");
        return NULL;
    }
    ctx->tables = next_tables;

    if (!table_state_load(&ctx->tables[ctx->table_count], ctx->db_root, table_name, error)) {
        return NULL;
    }

    ctx->table_count++;
    return &ctx->tables[ctx->table_count - 1];
}

int db_context_reload_table(DbContext *ctx, const char *table_name, SqlError *error) {
    int index;

    if (ctx == NULL || table_name == NULL) {
        sql_set_error(error, 0, 0, "db_context_reload_table received invalid arguments");
        return 0;
    }

    for (index = 0; index < ctx->table_count; index++) {
        if (strcmp(ctx->tables[index].name, table_name) == 0) {
            TableState reloaded = {0};

            if (!table_state_load(&reloaded, ctx->db_root, table_name, error)) {
                return 0;
            }

            table_state_destroy(&ctx->tables[index]);
            ctx->tables[index] = reloaded;
            return 1;
        }
    }

    sql_set_error(error, 0, 0, "table not loaded: %s", table_name);
    return 0;
}
