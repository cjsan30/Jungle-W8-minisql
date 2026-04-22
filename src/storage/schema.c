#include "schema.h"
#include <ctype.h>

static char *trim_in_place(char *text) {
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

static int parse_column_type(const char *type_name, ColumnType *type, SqlError *error) {
    if (strcmp(type_name, "int") == 0) {
        *type = COLUMN_TYPE_INT;
        return 1;
    }
    if (strcmp(type_name, "float") == 0) {
        *type = COLUMN_TYPE_FLOAT;
        return 1;
    }
    if (strcmp(type_name, "string") == 0) {
        *type = COLUMN_TYPE_STRING;
        return 1;
    }

    sql_set_error(error, 0, 0, "unsupported column type: %s", type_name);
    return 0;
}

static int parse_columns(char *value, Schema *schema, SqlError *error) {
    char *cursor = value;
    int count = 0;
    ColumnDef *columns = NULL;

    while (*cursor != '\0') {
        char *next = strchr(cursor, ',');
        char *separator;
        char *entry;
        char *name;
        char *type_name;
        ColumnDef *next_columns;

        if (next != NULL) {
            *next = '\0';
        }

        entry = trim_in_place(cursor);
        separator = strchr(entry, ':');
        if (separator == NULL) {
            free(columns);
            sql_set_error(error, 0, 0, "invalid column definition: %s", entry);
            return 0;
        }

        *separator = '\0';
        name = trim_in_place(entry);
        type_name = trim_in_place(separator + 1);
        next_columns = (ColumnDef *) realloc(columns, (size_t) (count + 1) * sizeof(ColumnDef));
        if (next_columns == NULL) {
            int index;
            for (index = 0; index < count; index++) {
                free(columns[index].name);
            }
            free(columns);
            sql_set_error(error, 0, 0, "failed to allocate schema columns");
            return 0;
        }
        columns = next_columns;
        memset(&columns[count], 0, sizeof(columns[count]));
        columns[count].name = sql_strdup(name);
        if (columns[count].name == NULL || !parse_column_type(type_name, &columns[count].type, error)) {
            int index;
            free(columns[count].name);
            for (index = 0; index < count; index++) {
                free(columns[index].name);
            }
            free(columns);
            if (error->message[0] == '\0') {
                sql_set_error(error, 0, 0, "failed to allocate schema column");
            }
            return 0;
        }
        count++;

        if (next == NULL) {
            break;
        }
        cursor = next + 1;
    }

    schema->columns = columns;
    schema->column_count = count;
    return 1;
}

int load_schema(const char *db_root, const char *table_name, Schema *schema, SqlError *error) {
    char path[SQL_PATH_BUFFER_SIZE];
    char *text;
    char *line;

    if (schema == NULL || table_name == NULL) {
        return 0;
    }

    memset(schema, 0, sizeof(*schema));
    schema->primary_key_index = -1;
    schema->table_name = sql_strdup(table_name);
    if (schema->table_name == NULL) {
        sql_set_error(error, 0, 0, "failed to allocate schema table name");
        return 0;
    }

    snprintf(path, sizeof(path), "%s/schema/%s.schema", db_root, table_name);
    text = sql_read_text_file(path, error);
    if (text == NULL) {
        free_schema(schema);
        return 0;
    }

    for (line = strtok(text, "\n"); line != NULL; line = strtok(NULL, "\n")) {
        char *trimmed = trim_in_place(line);
        char *separator;
        char *key;
        char *value;
        int index;

        if (*trimmed == '\0') {
            continue;
        }

        separator = strchr(trimmed, '=');
        if (separator == NULL) {
            free(text);
            free_schema(schema);
            sql_set_error(error, 0, 0, "invalid schema line: %s", trimmed);
            return 0;
        }

        *separator = '\0';
        key = trim_in_place(trimmed);
        value = trim_in_place(separator + 1);

        if (strcmp(key, "table") == 0) {
            free(schema->table_name);
            schema->table_name = sql_strdup(value);
            if (schema->table_name == NULL) {
                free(text);
                free_schema(schema);
                sql_set_error(error, 0, 0, "failed to allocate schema table name");
                return 0;
            }
        } else if (strcmp(key, "columns") == 0) {
            if (!parse_columns(value, schema, error)) {
                free(text);
                free_schema(schema);
                return 0;
            }
        } else if (strcmp(key, "pkey") == 0) {
            schema->primary_key = sql_strdup(value);
            if (schema->primary_key == NULL) {
                free(text);
                free_schema(schema);
                sql_set_error(error, 0, 0, "failed to allocate primary key");
                return 0;
            }
        } else if (strcmp(key, "autoincrement") == 0) {
            schema->autoincrement = strcmp(value, "true") == 0 ? 1 : 0;
        }

        if (schema->primary_key != NULL && schema->columns != NULL) {
            for (index = 0; index < schema->column_count; index++) {
                if (strcmp(schema->columns[index].name, schema->primary_key) == 0) {
                    schema->primary_key_index = index;
                    break;
                }
            }
        }
    }

    free(text);

    if (schema->column_count <= 0) {
        free_schema(schema);
        sql_set_error(error, 0, 0, "schema has no columns");
        return 0;
    }

    return 1;
}

void free_schema(Schema *schema) {
    int index;

    if (schema == NULL) {
        return;
    }

    for (index = 0; index < schema->column_count; index++) {
        free(schema->columns[index].name);
    }

    free(schema->columns);
    schema->columns = NULL;
    schema->column_count = 0;
    free(schema->table_name);
    schema->table_name = NULL;
    free(schema->primary_key);
    schema->primary_key = NULL;
    schema->primary_key_index = -1;
}
