#ifndef AST_H
#define AST_H

typedef enum {
    QUERY_INSERT,
    QUERY_SELECT
} QueryType;

typedef enum {
    VALUE_INT,
    VALUE_FLOAT,
    VALUE_STRING
} ValueType;

typedef struct {
    ValueType type;
    char *raw;
} Value;

typedef struct {
    char *column;
    Value value;
} Condition;

typedef struct {
    QueryType type;
    char *table_name;
    char **values;
    int value_count;
    int has_condition;
    Condition condition;
} Query;

#endif
