#ifndef PARSER_H
#define PARSER_H

#include "ast.h"
#include "common.h"

int parse_sql_text(const char *sql, Query *query, SqlError *error);

#endif
