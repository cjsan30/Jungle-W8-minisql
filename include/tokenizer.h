#ifndef TOKENIZER_H
#define TOKENIZER_H

#include "common.h"

int tokenize_sql(const char *sql, SqlError *error);

#endif
