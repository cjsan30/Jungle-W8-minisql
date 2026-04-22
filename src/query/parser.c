#include "parser.h"
#include "tokenizer.h"
#include <ctype.h>

static void skip_whitespace(const char **cursor) {
    while (**cursor != '\0' && isspace((unsigned char) **cursor)) {
        (*cursor)++;
    }
}

static int is_identifier_char(unsigned char ch) {
    return isalnum(ch) || ch == '_';
}

static int match_keyword(const char **cursor, const char *keyword) {
    const char *scan = *cursor;
    size_t index;

    skip_whitespace(&scan);
    for (index = 0; keyword[index] != '\0'; index++) {
        if (tolower((unsigned char) scan[index]) != tolower((unsigned char) keyword[index])) {
            return 0;
        }
    }

    if (is_identifier_char((unsigned char) scan[index])) {
        return 0;
    }

    *cursor = scan + index;
    return 1;
}

static char *parse_identifier(const char **cursor, SqlError *error) {
    const char *start;
    size_t length;
    char *identifier;

    skip_whitespace(cursor);
    start = *cursor;
    if (!is_identifier_char((unsigned char) **cursor)) {
        sql_set_error(error, 0, 0, "expected identifier");
        return NULL;
    }

    while (is_identifier_char((unsigned char) **cursor)) {
        (*cursor)++;
    }

    length = (size_t) (*cursor - start);
    identifier = (char *) malloc(length + 1U);
    if (identifier == NULL) {
        sql_set_error(error, 0, 0, "failed to allocate identifier");
        return NULL;
    }

    memcpy(identifier, start, length);
    identifier[length] = '\0';
    return identifier;
}

static char *parse_value(const char **cursor, SqlError *error) {
    const char *start;
    char quote;
    size_t length;
    char *value;

    skip_whitespace(cursor);
    if (**cursor == '\'' || **cursor == '"') {
        quote = **cursor;
        (*cursor)++;
        start = *cursor;
        while (**cursor != '\0' && **cursor != quote) {
            (*cursor)++;
        }
        if (**cursor != quote) {
            sql_set_error(error, 0, 0, "unterminated string literal");
            return NULL;
        }
        length = (size_t) (*cursor - start);
        (*cursor)++;
    } else {
        start = *cursor;
        while (**cursor != '\0' && **cursor != ',' && **cursor != ')' && **cursor != ';' &&
               !isspace((unsigned char) **cursor)) {
            (*cursor)++;
        }
        length = (size_t) (*cursor - start);
    }

    if (length == 0U) {
        sql_set_error(error, 0, 0, "expected value");
        return NULL;
    }

    value = (char *) malloc(length + 1U);
    if (value == NULL) {
        sql_set_error(error, 0, 0, "failed to allocate value");
        return NULL;
    }

    memcpy(value, start, length);
    value[length] = '\0';
    return value;
}

static void free_query_values(Query *query) {
    int index;

    if (query == NULL || query->values == NULL) {
        return;
    }

    for (index = 0; index < query->value_count; index++) {
        free(query->values[index]);
    }

    free(query->values);
    query->values = NULL;
    query->value_count = 0;
}

static void free_query_condition(Query *query) {
    if (query == NULL || !query->has_condition) {
        return;
    }

    free(query->condition.column);
    free(query->condition.value.raw);
    query->condition.column = NULL;
    query->condition.value.raw = NULL;
    query->has_condition = 0;
}

static int parse_condition(const char **cursor, Query *query, SqlError *error) {
    char *column;
    char *value;

    if (!match_keyword(cursor, "where")) {
        return 1;
    }

    column = parse_identifier(cursor, error);
    if (column == NULL) {
        return 0;
    }

    skip_whitespace(cursor);
    if (**cursor != '=') {
        free(column);
        sql_set_error(error, 0, 0, "expected '=' in WHERE clause");
        return 0;
    }
    (*cursor)++;

    value = parse_value(cursor, error);
    if (value == NULL) {
        free(column);
        return 0;
    }

    query->has_condition = 1;
    query->condition.column = column;
    query->condition.value.raw = value;
    query->condition.value.type = VALUE_STRING;
    return 1;
}

static int parse_select(const char *sql, Query *query, SqlError *error) {
    const char *cursor = sql;

    if (!match_keyword(&cursor, "select")) {
        sql_set_error(error, 0, 0, "expected SELECT");
        return 0;
    }

    skip_whitespace(&cursor);
    if (*cursor != '*') {
        sql_set_error(error, 0, 0, "only SELECT * is supported");
        return 0;
    }
    cursor++;

    if (!match_keyword(&cursor, "from")) {
        sql_set_error(error, 0, 0, "expected FROM");
        return 0;
    }

    query->table_name = parse_identifier(&cursor, error);
    if (query->table_name == NULL) {
        return 0;
    }

    if (!parse_condition(&cursor, query, error)) {
        return 0;
    }

    skip_whitespace(&cursor);
    if (*cursor == ';') {
        cursor++;
    }

    skip_whitespace(&cursor);
    if (*cursor != '\0') {
        sql_set_error(error, 0, 0, "unexpected trailing SQL");
        return 0;
    }

    query->type = QUERY_SELECT;
    return 1;
}

static int parse_insert(const char *sql, Query *query, SqlError *error) {
    const char *cursor = sql;
    char **values = NULL;
    int value_count = 0;
    int value_capacity = 0;

    if (!match_keyword(&cursor, "insert")) {
        sql_set_error(error, 0, 0, "expected INSERT");
        return 0;
    }

    if (!match_keyword(&cursor, "into")) {
        sql_set_error(error, 0, 0, "expected INTO");
        return 0;
    }

    query->table_name = parse_identifier(&cursor, error);
    if (query->table_name == NULL) {
        return 0;
    }

    if (!match_keyword(&cursor, "values")) {
        sql_set_error(error, 0, 0, "expected VALUES");
        return 0;
    }

    skip_whitespace(&cursor);
    if (*cursor != '(') {
        sql_set_error(error, 0, 0, "expected '(' after VALUES");
        return 0;
    }
    cursor++;

    for (;;) {
        char *value;
        char **next_values;

        value = parse_value(&cursor, error);
        if (value == NULL) {
            int index;
            for (index = 0; index < value_count; index++) {
                free(values[index]);
            }
            free(values);
            return 0;
        }

        if (value_count == value_capacity) {
            value_capacity = value_capacity == 0 ? 4 : value_capacity * 2;
            next_values = (char **) realloc(values, (size_t) value_capacity * sizeof(char *));
            if (next_values == NULL) {
                int index;
                free(value);
                for (index = 0; index < value_count; index++) {
                    free(values[index]);
                }
                free(values);
                sql_set_error(error, 0, 0, "failed to allocate insert values");
                return 0;
            }
            values = next_values;
        }

        values[value_count++] = value;
        skip_whitespace(&cursor);
        if (*cursor == ',') {
            cursor++;
            continue;
        }
        if (*cursor == ')') {
            cursor++;
            break;
        }

        sql_set_error(error, 0, 0, "expected ',' or ')' in VALUES list");
        while (value_count > 0) {
            free(values[--value_count]);
        }
        free(values);
        return 0;
    }

    skip_whitespace(&cursor);
    if (*cursor == ';') {
        cursor++;
    }

    skip_whitespace(&cursor);
    if (*cursor != '\0') {
        while (value_count > 0) {
            free(values[--value_count]);
        }
        free(values);
        sql_set_error(error, 0, 0, "unexpected trailing SQL");
        return 0;
    }

    query->type = QUERY_INSERT;
    query->values = values;
    query->value_count = value_count;
    return 1;
}

int parse_sql_text(const char *sql, Query *query, SqlError *error) {
    if (sql == NULL || sql[0] == '\0') {
        sql_set_error(error, 0, 0, "sql must not be empty");
        return 0;
    }

    if (query == NULL) {
        sql_set_error(error, 0, 0, "query must not be null");
        return 0;
    }

    if (!tokenize_sql(sql, error)) {
        return 0;
    }

    memset(query, 0, sizeof(*query));

    if (parse_select(sql, query, error)) {
        return 1;
    }

    free(query->table_name);
    query->table_name = NULL;
    free_query_values(query);
    free_query_condition(query);

    if (parse_insert(sql, query, error)) {
        return 1;
    }

    free(query->table_name);
    query->table_name = NULL;
    free_query_values(query);
    free_query_condition(query);
    sql_set_error(error, 0, 0, "unsupported SQL statement");
    return 0;
}
