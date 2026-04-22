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

char *sql_json_quote(const char *src, SqlError *error) {
    const char *text = src != NULL ? src : "";
    size_t input_length = strlen(text);
    size_t output_capacity;
    char *quoted;
    size_t input_index;
    size_t output_index = 0;

    if (input_length > (((size_t) -1) - 3U) / 6U) {
        sql_set_error(error, 0, 0, "JSON string is too large to quote");
        return NULL;
    }

    output_capacity = (input_length * 6U) + 3U;
    quoted = (char *) malloc(output_capacity);
    if (quoted == NULL) {
        sql_set_error(error, 0, 0, "failed to allocate JSON string");
        return NULL;
    }

    quoted[output_index++] = '"';
    for (input_index = 0; input_index < input_length; input_index++) {
        unsigned char ch = (unsigned char) text[input_index];

        switch (ch) {
            case '"':
                quoted[output_index++] = '\\';
                quoted[output_index++] = '"';
                break;
            case '\\':
                quoted[output_index++] = '\\';
                quoted[output_index++] = '\\';
                break;
            case '\b':
                quoted[output_index++] = '\\';
                quoted[output_index++] = 'b';
                break;
            case '\f':
                quoted[output_index++] = '\\';
                quoted[output_index++] = 'f';
                break;
            case '\n':
                quoted[output_index++] = '\\';
                quoted[output_index++] = 'n';
                break;
            case '\r':
                quoted[output_index++] = '\\';
                quoted[output_index++] = 'r';
                break;
            case '\t':
                quoted[output_index++] = '\\';
                quoted[output_index++] = 't';
                break;
            default:
                if (ch < 0x20U) {
                    (void) snprintf(quoted + output_index, 7U, "\\u%04x", ch);
                    output_index += 6U;
                    break;
                }

                quoted[output_index++] = (char) ch;
                break;
        }
    }

    quoted[output_index++] = '"';
    quoted[output_index] = '\0';
    return quoted;
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
