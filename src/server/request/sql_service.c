#include "sql_service.h"
#include "executor.h"
#include "parser.h"
#include <ctype.h>

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} JsonBuffer;

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

static char *read_stream_payload(FILE *stream, SqlError *error) {
    long length;
    size_t bytes_read;
    char *payload;

    if (stream == NULL) {
        sql_set_error(error, 0, 0, "SQL result stream must not be null");
        return NULL;
    }
    if (fflush(stream) != 0) {
        sql_set_error(error, 0, 0, "failed to flush SQL result stream");
        return NULL;
    }
    if (fseek(stream, 0L, SEEK_END) != 0) {
        sql_set_error(error, 0, 0, "failed to seek SQL result stream");
        return NULL;
    }

    length = ftell(stream);
    if (length < 0) {
        sql_set_error(error, 0, 0, "failed to measure SQL result stream");
        return NULL;
    }
    if (fseek(stream, 0L, SEEK_SET) != 0) {
        sql_set_error(error, 0, 0, "failed to rewind SQL result stream");
        return NULL;
    }

    payload = (char *) malloc((size_t) length + 1U);
    if (payload == NULL) {
        sql_set_error(error, 0, 0, "failed to allocate SQL result payload");
        return NULL;
    }

    bytes_read = fread(payload, 1U, (size_t) length, stream);
    if (bytes_read != (size_t) length) {
        free(payload);
        sql_set_error(error, 0, 0, "failed to read SQL result payload");
        return NULL;
    }

    payload[bytes_read] = '\0';
    return payload;
}

static const char *skip_sql_prefix(const char *sql_text) {
    const char *cursor = sql_text;

    for (;;) {
        while (*cursor != '\0' && isspace((unsigned char) *cursor)) {
            cursor++;
        }

        if (cursor[0] == '-' && cursor[1] == '-') {
            cursor += 2;
            while (*cursor != '\0' && *cursor != '\n') {
                cursor++;
            }
            continue;
        }

        if (*cursor == ';') {
            cursor++;
            continue;
        }

        return cursor;
    }
}

static int is_sql_identifier_char(unsigned char ch) {
    return isalnum(ch) || ch == '_';
}

static int matches_keyword(const char *text, const char *keyword) {
    size_t index;

    for (index = 0; keyword[index] != '\0'; index++) {
        if (toupper((unsigned char) text[index]) != keyword[index]) {
            return 0;
        }
    }

    return !is_sql_identifier_char((unsigned char) text[index]);
}

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

static int find_column_index(const Schema *schema, const char *column_name) {
    int index;

    for (index = 0; index < schema->column_count; index++) {
        if (strcmp(schema->columns[index].name, column_name) == 0) {
            return index;
        }
    }

    return -1;
}

static size_t trimmed_message_length(const char *message) {
    size_t length;

    if (message == NULL) {
        return 0U;
    }

    length = strlen(message);
    while (length > 0U && (message[length - 1U] == '\n' || message[length - 1U] == '\r')) {
        length--;
    }
    return length;
}

static char *trimmed_message_copy(const char *message, SqlError *error) {
    size_t length = trimmed_message_length(message);
    char *copy = (char *) malloc(length + 1U);

    if (copy == NULL) {
        sql_set_error(error, 0, 0, "failed to allocate trimmed SQL message");
        return NULL;
    }

    if (length > 0U) {
        memcpy(copy, message, length);
    }
    copy[length] = '\0';
    return copy;
}

static int json_buffer_reserve(JsonBuffer *buffer, size_t additional, SqlError *error) {
    size_t needed;
    size_t next_capacity;
    char *next_data;

    if (buffer == NULL) {
        sql_set_error(error, 0, 0, "JSON buffer must not be null");
        return 0;
    }
    if (additional > (((size_t) -1) - buffer->length) - 1U) {
        sql_set_error(error, 0, 0, "JSON payload is too large");
        return 0;
    }

    needed = buffer->length + additional + 1U;
    if (needed <= buffer->capacity) {
        return 1;
    }

    next_capacity = buffer->capacity == 0U ? 128U : buffer->capacity;
    while (next_capacity < needed) {
        if (next_capacity > ((size_t) -1) / 2U) {
            next_capacity = needed;
            break;
        }
        next_capacity *= 2U;
    }

    next_data = (char *) realloc(buffer->data, next_capacity);
    if (next_data == NULL) {
        sql_set_error(error, 0, 0, "failed to expand JSON payload buffer");
        return 0;
    }

    buffer->data = next_data;
    buffer->capacity = next_capacity;
    return 1;
}

static int json_buffer_append_raw(JsonBuffer *buffer, const char *text, SqlError *error) {
    size_t text_length;

    if (text == NULL) {
        return 1;
    }

    text_length = strlen(text);
    if (!json_buffer_reserve(buffer, text_length, error)) {
        return 0;
    }

    memcpy(buffer->data + buffer->length, text, text_length);
    buffer->length += text_length;
    buffer->data[buffer->length] = '\0';
    return 1;
}

static int json_buffer_append_quoted(JsonBuffer *buffer, const char *text, SqlError *error) {
    char *quoted = sql_json_quote(text, error);
    int ok;

    if (quoted == NULL) {
        return 0;
    }

    ok = json_buffer_append_raw(buffer, quoted, error);
    free(quoted);
    return ok;
}

static char *json_buffer_finalize(JsonBuffer *buffer) {
    char *payload;

    if (buffer == NULL) {
        return NULL;
    }

    if (buffer->data == NULL) {
        payload = sql_strdup("");
    } else {
        payload = buffer->data;
        buffer->data = NULL;
    }

    buffer->length = 0U;
    buffer->capacity = 0U;
    return payload;
}

static char *build_error_payload(const char *message, SqlError *error) {
    JsonBuffer json = {0};
    char *trimmed = NULL;
    char *payload = NULL;

    trimmed = trimmed_message_copy(message != NULL ? message : "unknown SQL error", error);
    if (trimmed == NULL) {
        return NULL;
    }

    if (!json_buffer_append_raw(&json, "{\"ok\":false,\"error\":{\"message\":", error) ||
        !json_buffer_append_quoted(&json, trimmed, error) ||
        !json_buffer_append_raw(&json, "}}", error)) {
        free(json.data);
        free(trimmed);
        return NULL;
    }

    payload = json_buffer_finalize(&json);
    free(trimmed);
    if (payload == NULL) {
        sql_set_error(error, 0, 0, "failed to finalize JSON error payload");
    }
    return payload;
}

static int append_row_object(JsonBuffer *json, const TableState *table, const Row *row, SqlError *error) {
    int column_index;

    if (!json_buffer_append_raw(json, "{", error)) {
        return 0;
    }

    for (column_index = 0; column_index < table->schema.column_count; column_index++) {
        const ColumnDef *column = &table->schema.columns[column_index];
        const char *field = row->fields[column_index];

        if (column_index > 0 && !json_buffer_append_raw(json, ",", error)) {
            return 0;
        }
        if (!json_buffer_append_quoted(json, column->name, error) ||
            !json_buffer_append_raw(json, ":", error)) {
            return 0;
        }

        if (column->type == COLUMN_TYPE_STRING) {
            if (!json_buffer_append_quoted(json, field, error)) {
                return 0;
            }
            continue;
        }

        if (column->type == COLUMN_TYPE_INT && !is_integer_string(field)) {
            sql_set_error(error, 0, 0, "invalid persisted int for column %s", column->name);
            return 0;
        }
        if (column->type == COLUMN_TYPE_FLOAT && !is_float_string(field)) {
            sql_set_error(error, 0, 0, "invalid persisted float for column %s", column->name);
            return 0;
        }
        if (!json_buffer_append_raw(json, field, error)) {
            return 0;
        }
    }

    return json_buffer_append_raw(json, "}", error);
}

static char *build_select_payload(DbContext *ctx, const Query *query, SqlError *error) {
    JsonBuffer json = {0};
    TableState *table;
    int first_row = 1;

    table = db_context_get_table(ctx, query->table_name, error);
    if (table == NULL) {
        return NULL;
    }

    if (!json_buffer_append_raw(&json, "{\"ok\":true,\"type\":\"select\",\"table\":", error) ||
        !json_buffer_append_quoted(&json, query->table_name, error) ||
        !json_buffer_append_raw(&json, ",\"rows\":[", error)) {
        free(json.data);
        return NULL;
    }

    if (!query->has_condition) {
        int row_index;

        for (row_index = 0; row_index < table->rowset.row_count; row_index++) {
            if (!first_row && !json_buffer_append_raw(&json, ",", error)) {
                free(json.data);
                return NULL;
            }
            if (!append_row_object(&json, table, &table->rowset.rows[row_index], error)) {
                free(json.data);
                return NULL;
            }
            first_row = 0;
        }
    } else if (strcmp(query->condition.column, "id") == 0) {
        int matched_row_index = -1;

        if (!is_integer_string(query->condition.value.raw)) {
            free(json.data);
            sql_set_error(error, 0, 0, "WHERE id requires an integer value");
            return NULL;
        }

        if (bptree_search(table->index, atoi(query->condition.value.raw), &matched_row_index)) {
            if (!append_row_object(&json, table, &table->rowset.rows[matched_row_index], error)) {
                free(json.data);
                return NULL;
            }
        }
    } else {
        int column_index = find_column_index(&table->schema, query->condition.column);
        int row_index;

        if (column_index < 0) {
            free(json.data);
            sql_set_error(error, 0, 0, "unknown column in WHERE clause: %s", query->condition.column);
            return NULL;
        }

        for (row_index = 0; row_index < table->rowset.row_count; row_index++) {
            if (strcmp(table->rowset.rows[row_index].fields[column_index], query->condition.value.raw) != 0) {
                continue;
            }

            if (!first_row && !json_buffer_append_raw(&json, ",", error)) {
                free(json.data);
                return NULL;
            }
            if (!append_row_object(&json, table, &table->rowset.rows[row_index], error)) {
                free(json.data);
                return NULL;
            }
            first_row = 0;
        }
    }

    if (!json_buffer_append_raw(&json, "]}", error)) {
        free(json.data);
        return NULL;
    }

    return json_buffer_finalize(&json);
}

static char *build_insert_payload(const Query *query, const char *message, SqlError *error) {
    JsonBuffer json = {0};
    char *trimmed = trimmed_message_copy(message, error);
    char *payload;

    if (trimmed == NULL) {
        return NULL;
    }

    if (!json_buffer_append_raw(&json, "{\"ok\":true,\"type\":\"insert\",\"table\":", error) ||
        !json_buffer_append_quoted(&json, query->table_name, error) ||
        !json_buffer_append_raw(&json, ",\"affected_rows\":1,\"message\":", error) ||
        !json_buffer_append_quoted(&json, trimmed, error) ||
        !json_buffer_append_raw(&json, "}", error)) {
        free(json.data);
        free(trimmed);
        return NULL;
    }

    payload = json_buffer_finalize(&json);
    free(trimmed);
    if (payload == NULL) {
        sql_set_error(error, 0, 0, "failed to finalize JSON insert payload");
    }
    return payload;
}

SqlService *sql_service_create(const char *db_root, SqlError *error) {
    SqlService *service;
    size_t root_length;

    if (db_root == NULL || db_root[0] == '\0') {
        sql_set_error(error, 0, 0, "sql_service_create requires a db_root");
        return NULL;
    }

    root_length = strlen(db_root);
    if (root_length >= sizeof(service->db_root)) {
        sql_set_error(error, 0, 0, "db_root path is too long");
        return NULL;
    }

    service = (SqlService *) malloc(sizeof(*service));
    if (service == NULL) {
        sql_set_error(error, 0, 0, "failed to allocate SQL service");
        return NULL;
    }

    memset(service, 0, sizeof(*service));
    memcpy(service->db_root, db_root, root_length + 1U);
    service->db_context = db_context_create(db_root, error);
    if (service->db_context == NULL) {
        free(service);
        return NULL;
    }
    return service;
}

int sql_service_classify_operation(const char *sql_text, SqlOperationKind *operation, SqlError *error) {
    const char *cursor;

    if (sql_text == NULL || operation == NULL) {
        sql_set_error(error, 0, 0, "sql_service_classify_operation received invalid arguments");
        return 0;
    }

    cursor = skip_sql_prefix(sql_text);
    if (*cursor == '\0') {
        *operation = SQL_OPERATION_UNKNOWN;
        sql_set_error(error, 0, 0, "sql_service_classify_operation received empty SQL");
        return 0;
    }

    if (matches_keyword(cursor, "SELECT")) {
        *operation = SQL_OPERATION_READ;
        return 1;
    }

    if (matches_keyword(cursor, "INSERT")) {
        *operation = SQL_OPERATION_WRITE;
        return 1;
    }

    *operation = SQL_OPERATION_UNKNOWN;
    sql_set_error(error, 0, 0, "sql_service_classify_operation could not classify SQL");
    return 0;
}

SqlServiceResult sql_service_execute(SqlService *service, const char *sql_text, SqlError *error) {
    SqlServiceResult result = {0};
    Query query = {0};
    FILE *stream = NULL;
    char *message = NULL;

    if (service == NULL || sql_text == NULL) {
        sql_set_error(error, 0, 0, "sql_service_execute received invalid arguments");
        result.payload = build_error_payload("sql_service_execute received invalid arguments", error);
        return result;
    }
    if (service->db_context == NULL) {
        sql_set_error(error, 0, 0, "sql_service_execute requires an initialized db_context");
        result.payload = build_error_payload("SQL service is not initialized.", error);
        return result;
    }

    if (!parse_sql_text(sql_text, &query, error)) {
        result.payload = build_error_payload(error != NULL ? error->message : "failed to parse SQL", error);
        return result;
    }

    if (query.type == QUERY_SELECT) {
        result.payload = build_select_payload(service->db_context, &query, error);
        if (result.payload != NULL) {
            result.ok = 1;
        } else {
            result.payload = build_error_payload(error != NULL ? error->message : "failed to execute select query", error);
        }
        free_query(&query);
        return result;
    }

    if (query.type != QUERY_INSERT) {
        sql_set_error(error, 0, 0, "unsupported query type");
        result.payload = build_error_payload(error != NULL ? error->message : "unsupported query type", error);
        free_query(&query);
        return result;
    }

    stream = tmpfile();
    if (stream == NULL) {
        sql_set_error(error, 0, 0, "failed to create SQL result stream");
        result.payload = build_error_payload("Failed to create SQL result stream.", error);
        free_query(&query);
        return result;
    }

    if (!execute_query(&query, service->db_context, stream, error)) {
        result.payload = build_error_payload(error != NULL ? error->message : "failed to execute insert query", error);
        fclose(stream);
        free_query(&query);
        return result;
    }

    message = read_stream_payload(stream, error);
    fclose(stream);
    stream = NULL;
    if (message == NULL) {
        result.payload = build_error_payload(error != NULL ? error->message : "failed to read SQL result payload", error);
        free_query(&query);
        return result;
    }

    result.payload = build_insert_payload(&query, message, error);
    free(message);
    free_query(&query);
    if (result.payload == NULL) {
        result.payload = build_error_payload(error != NULL ? error->message : "failed to build insert response", error);
        return result;
    }

    result.ok = 1;
    return result;
}

void sql_service_result_destroy(SqlServiceResult *result) {
    if (result == NULL) {
        return;
    }

    free(result->payload);
    result->payload = NULL;
    result->ok = 0;
}

void sql_service_destroy(SqlService *service) {
    if (service == NULL) {
        return;
    }

    if (service->db_context != NULL) {
        db_context_destroy(service->db_context);
        service->db_context = NULL;
    }

    free(service);
}
