#include "common.h"

void sql_set_error(SqlError *error, int line, int column, const char *fmt, ...) {
    va_list args;

    if (error == NULL) {
        return;
    }

    error->line = line;
    error->column = column;

    va_start(args, fmt);
    vsnprintf(error->message, sizeof(error->message), fmt, args);
    va_end(args);
}

char *sql_strdup(const char *src) {
    char *copy;
    size_t length;

    if (src == NULL) {
        return NULL;
    }

    length = strlen(src) + 1;
    copy = (char *) malloc(length);
    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, src, length);
    return copy;
}

char *sql_read_text_file(const char *path, SqlError *error) {
    FILE *file;
    long length;
    size_t bytes_read;
    char *buffer;

    if (path == NULL) {
        sql_set_error(error, 0, 0, "path must not be null");
        return NULL;
    }

    file = fopen(path, "rb");
    if (file == NULL) {
        sql_set_error(error, 0, 0, "failed to open file: %s", path);
        return NULL;
    }

    if (fseek(file, 0L, SEEK_END) != 0) {
        fclose(file);
        sql_set_error(error, 0, 0, "failed to seek file: %s", path);
        return NULL;
    }

    length = ftell(file);
    if (length < 0) {
        fclose(file);
        sql_set_error(error, 0, 0, "failed to tell file length: %s", path);
        return NULL;
    }

    if (fseek(file, 0L, SEEK_SET) != 0) {
        fclose(file);
        sql_set_error(error, 0, 0, "failed to rewind file: %s", path);
        return NULL;
    }

    buffer = (char *) malloc((size_t) length + 1U);
    if (buffer == NULL) {
        fclose(file);
        sql_set_error(error, 0, 0, "failed to allocate file buffer");
        return NULL;
    }

    bytes_read = fread(buffer, 1U, (size_t) length, file);
    fclose(file);

    if (bytes_read != (size_t) length) {
        free(buffer);
        sql_set_error(error, 0, 0, "failed to read file: %s", path);
        return NULL;
    }

    buffer[bytes_read] = '\0';
    return buffer;
}
