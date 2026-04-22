#include "storage.h"
#include <ctype.h>

static void build_table_path(char *buffer, size_t buffer_size, const char *db_root, const char *table_name) {
    snprintf(buffer, buffer_size, "%s/tables/%s.csv", db_root, table_name);
}

static char *trim_field(char *text) {
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

static int rowset_append(RowSet *rowset, char **fields, int field_count, SqlError *error) {
    Row *next_rows;

    if (rowset->row_count == rowset->row_capacity) {
        rowset->row_capacity = rowset->row_capacity == 0 ? 8 : rowset->row_capacity * 2;
        next_rows = (Row *) realloc(rowset->rows, (size_t) rowset->row_capacity * sizeof(Row));
        if (next_rows == NULL) {
            sql_set_error(error, 0, 0, "failed to allocate rowset");
            return 0;
        }
        rowset->rows = next_rows;
    }

    rowset->rows[rowset->row_count].fields = fields;
    rowset->rows[rowset->row_count].field_count = field_count;
    rowset->row_count++;
    return 1;
}

static void free_fields(char **fields, int count) {
    while (count > 0) {
        free(fields[--count]);
    }
    free(fields);
}

static int append_field(char ***fields, int *count, const char *value, SqlError *error) {
    char **next_fields;

    next_fields = (char **) realloc(*fields, (size_t) (*count + 1) * sizeof(char *));
    if (next_fields == NULL) {
        sql_set_error(error, 0, 0, "failed to allocate row fields");
        return 0;
    }
    *fields = next_fields;

    (*fields)[*count] = sql_strdup(value);
    if ((*fields)[*count] == NULL) {
        sql_set_error(error, 0, 0, "failed to allocate row field");
        return 0;
    }

    (*count)++;
    return 1;
}

static int parse_csv_line(const char *line, int expected_fields, char ***fields_out, SqlError *error) {
    char **fields = NULL;
    int field_count = 0;
    size_t line_length = strlen(line);
    char *buffer = (char *) malloc(line_length + 1U);
    size_t buffer_length = 0;
    int in_quotes = 0;
    size_t index;

    if (buffer == NULL) {
        sql_set_error(error, 0, 0, "failed to allocate CSV parse buffer");
        return 0;
    }

    for (index = 0; index <= line_length; index++) {
        char ch = line[index];

        if (in_quotes) {
            if (ch == '"') {
                if (line[index + 1] == '"') {
                    buffer[buffer_length++] = '"';
                    index++;
                } else {
                    in_quotes = 0;
                }
            } else if (ch == '\0') {
                free(buffer);
                free_fields(fields, field_count);
                sql_set_error(error, 0, 0, "unterminated quoted CSV field");
                return 0;
            } else {
                buffer[buffer_length++] = ch;
            }
            continue;
        }

        if (ch == '"') {
            in_quotes = 1;
            continue;
        }

        if (ch == ',' || ch == '\0') {
            buffer[buffer_length] = '\0';
            {
                char *trimmed = trim_field(buffer);
                if (!append_field(&fields, &field_count, trimmed, error)) {
                    free(buffer);
                    free_fields(fields, field_count);
                    return 0;
                }
            }
            buffer_length = 0;
            continue;
        }

        buffer[buffer_length++] = ch;
    }

    free(buffer);

    if (field_count != expected_fields) {
        free_fields(fields, field_count);
        sql_set_error(error, 0, 0, "row field count mismatch");
        return 0;
    }

    *fields_out = fields;
    return 1;
}

static int write_csv_field(FILE *file, const char *value, SqlError *error) {
    const char *cursor;

    if (fputc('"', file) == EOF) {
        sql_set_error(error, 0, 0, "failed to write CSV quote");
        return 0;
    }

    for (cursor = value; *cursor != '\0'; cursor++) {
        if (*cursor == '"') {
            if (fputc('"', file) == EOF || fputc('"', file) == EOF) {
                sql_set_error(error, 0, 0, "failed to escape CSV quote");
                return 0;
            }
        } else if (fputc(*cursor, file) == EOF) {
            sql_set_error(error, 0, 0, "failed to write CSV field");
            return 0;
        }
    }

    if (fputc('"', file) == EOF) {
        sql_set_error(error, 0, 0, "failed to write CSV quote");
        return 0;
    }

    return 1;
}

int append_csv_row(const char *db_root, const char *table_name, char **fields, int field_count, SqlError *error) {
    char path[SQL_PATH_BUFFER_SIZE];
    FILE *file;
    int index;

    if (db_root == NULL || table_name == NULL || fields == NULL || field_count <= 0) {
        sql_set_error(error, 0, 0, "append_csv_row received invalid arguments");
        return 0;
    }

    build_table_path(path, sizeof(path), db_root, table_name);
    file = fopen(path, "a");
    if (file == NULL) {
        sql_set_error(error, 0, 0, "failed to open table file: %s", path);
        return 0;
    }

    for (index = 0; index < field_count; index++) {
        if (index > 0) {
            fputc(',', file);
        }
        if (!write_csv_field(file, fields[index], error)) {
            fclose(file);
            return 0;
        }
    }
    fputc('\n', file);

    if (fclose(file) != 0) {
        sql_set_error(error, 0, 0, "failed to flush table file: %s", path);
        return 0;
    }

    return 1;
}

int read_csv_rows(const char *db_root, const char *table_name, int expected_fields, RowSet *rowset, SqlError *error) {
    char path[SQL_PATH_BUFFER_SIZE];
    FILE *file;
    char line_buffer[8192];

    if (rowset == NULL) {
        return 0;
    }

    memset(rowset, 0, sizeof(*rowset));
    rowset->column_count = expected_fields;

    build_table_path(path, sizeof(path), db_root, table_name);
    file = fopen(path, "r");
    if (file == NULL) {
        sql_set_error(error, 0, 0, "failed to open table file: %s", path);
        return 0;
    }

    while (fgets(line_buffer, sizeof(line_buffer), file) != NULL) {
        size_t line_length;
        char **fields = NULL;

        line_length = strlen(line_buffer);
        while (line_length > 0U && (line_buffer[line_length - 1] == '\n' || line_buffer[line_length - 1] == '\r')) {
            line_buffer[--line_length] = '\0';
        }

        if (line_buffer[0] == '\0') {
            continue;
        }

        if (!parse_csv_line(line_buffer, expected_fields, &fields, error) ||
            !rowset_append(rowset, fields, expected_fields, error)) {
            fclose(file);
            free_rowset(rowset);
            return 0;
        }
    }

    fclose(file);
    return 1;
}

void free_rowset(RowSet *rowset) {
    int row_index;
    int field_index;

    if (rowset == NULL) {
        return;
    }

    for (row_index = 0; row_index < rowset->row_count; row_index++) {
        for (field_index = 0; field_index < rowset->rows[row_index].field_count; field_index++) {
            free(rowset->rows[row_index].fields[field_index]);
        }
        free(rowset->rows[row_index].fields);
    }

    free(rowset->rows);
    memset(rowset, 0, sizeof(*rowset));
}
