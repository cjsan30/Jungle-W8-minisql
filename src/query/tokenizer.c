#include "tokenizer.h"
#include <ctype.h>

int tokenize_sql(const char *sql, SqlError *error) {
    const unsigned char *cursor = (const unsigned char *) sql;

    if (sql == NULL) {
        sql_set_error(error, 0, 0, "sql must not be empty");
        return 0;
    }

    while (*cursor != '\0') {
        if (!isspace(*cursor)) {
            return 1;
        }
        cursor++;
    }

    sql_set_error(error, 0, 0, "sql must not be empty");
    return 0;
}
