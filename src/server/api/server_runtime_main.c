#include "server.h"

static void print_server_usage(FILE *out, const char *program_name) {
    fprintf(out, "usage: %s [--host 0.0.0.0] [--port 8080] [--db ./data]\n", program_name);
}

static int parse_port_value(const char *text, int *out_port, SqlError *error) {
    char *end = NULL;
    long parsed;

    if (text == NULL || out_port == NULL) {
        sql_set_error(error, 0, 0, "server port argument is invalid");
        return 0;
    }

    parsed = strtol(text, &end, 10);
    if (end == text || *end != '\0') {
        sql_set_error(error, 0, 0, "server port must be numeric");
        return 0;
    }
    if (parsed <= 0 || parsed > 65535) {
        sql_set_error(error, 0, 0, "server port must be between 1 and 65535");
        return 0;
    }

    *out_port = (int) parsed;
    return 1;
}

static int parse_server_args(int argc, char **argv, ServerConfig *config, SqlError *error) {
    int index;

    if (config == NULL) {
        sql_set_error(error, 0, 0, "server config must not be null");
        return 0;
    }

    server_config_set_defaults(config);
    for (index = 1; index < argc; index++) {
        if (strcmp(argv[index], "--host") == 0 && index + 1 < argc) {
            if (strlen(argv[index + 1]) >= sizeof(config->host)) {
                sql_set_error(error, 0, 0, "server host is too long");
                return 0;
            }
            memcpy(config->host, argv[index + 1], strlen(argv[index + 1]) + 1U);
            index++;
            continue;
        }
        if (strcmp(argv[index], "--port") == 0 && index + 1 < argc) {
            if (!parse_port_value(argv[index + 1], &config->port, error)) {
                return 0;
            }
            index++;
            continue;
        }
        if (strcmp(argv[index], "--db") == 0 && index + 1 < argc) {
            if (strlen(argv[index + 1]) >= sizeof(config->db_root)) {
                sql_set_error(error, 0, 0, "server db path is too long");
                return 0;
            }
            memcpy(config->db_root, argv[index + 1], strlen(argv[index + 1]) + 1U);
            index++;
            continue;
        }
        if (strcmp(argv[index], "--help") == 0) {
            print_server_usage(stdout, argv[0]);
            return 0;
        }

        sql_set_error(error, 0, 0, "unknown server argument: %s", argv[index]);
        return 0;
    }

    return 1;
}

int main(int argc, char **argv) {
    ServerConfig config;
    SqlError error = {0};
    DbServer *server;

    if (!parse_server_args(argc, argv, &config, &error)) {
        if (error.message[0] != '\0') {
            print_server_usage(stderr, argv[0]);
            fprintf(stderr, "error: %s\n", error.message);
            return 1;
        }
        return 0;
    }

    server = server_create(&config, &error);
    if (server == NULL) {
        fprintf(stderr, "error: %s\n", error.message);
        return 1;
    }

    if (!server_start(server, &error)) {
        fprintf(stderr, "error: %s\n", error.message);
        server_destroy(server);
        return 1;
    }

    server_destroy(server);
    return 0;
}
