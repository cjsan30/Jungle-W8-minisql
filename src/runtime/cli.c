#include "cli.h"

int parse_cli_args(int argc, char **argv, CliOptions *options, SqlError *error) {
    int index;

    if (options == NULL) {
        sql_set_error(error, 0, 0, "options must not be null");
        return 0;
    }

    options->db_root = "./data";

    for (index = 1; index < argc; index++) {
        if (strcmp(argv[index], "--sql") == 0 && index + 1 < argc) {
            options->sql_path = argv[++index];
        } else if (strcmp(argv[index], "--query") == 0 && index + 1 < argc) {
            options->query_text = argv[++index];
        } else if (strcmp(argv[index], "--param") == 0 && index + 1 < argc) {
            options->param_value = argv[++index];
        } else if (strcmp(argv[index], "--db") == 0 && index + 1 < argc) {
            options->db_root = argv[++index];
        } else if (strcmp(argv[index], "--bench") == 0 && index + 1 < argc) {
            options->bench_rows = atol(argv[++index]);
        } else if (strcmp(argv[index], "--explain") == 0) {
            options->explain = 1;
        } else {
            sql_set_error(error, 0, 0, "unknown argument: %s", argv[index]);
            return 0;
        }
    }

    if (options->sql_path != NULL && options->query_text != NULL) {
        sql_set_error(error, 0, 0, "--sql and --query cannot be used together");
        return 0;
    }

    if (options->sql_path == NULL && options->query_text == NULL && options->bench_rows <= 0) {
        sql_set_error(error, 0, 0, "either --sql, --query, or --bench must be provided");
        return 0;
    }

    return 1;
}

void print_usage(FILE *out, const char *program_name) {
    fprintf(out, "usage: %s --sql query.sql [--db ./data] [--explain]\n", program_name);
    fprintf(out, "       %s --query \"SELECT * FROM users;\" [--param 2] [--db ./data] [--explain]\n", program_name);
    fprintf(out, "       %s --bench 1000000 [--db ./data]\n", program_name);
}
