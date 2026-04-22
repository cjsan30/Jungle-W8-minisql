#include "request_handler.h"
#include <ctype.h>

typedef struct {
    SqlService *sql_service;
    const char *sql_text;
    SqlServiceResult result;
} RequestExecutionContext;

static int is_blank_text(const char *text, size_t length) {
    size_t index;

    if (text == NULL) {
        return 1;
    }

    for (index = 0; index < length; index++) {
        if (!isspace((unsigned char) text[index])) {
            return 0;
        }
    }
    return 1;
}

static int set_json_response(HttpResponse *response, int status_code, const char *body, SqlError *error) {
    HttpResponse built = {0};
    const char *payload = body != NULL ? body : "";

    built.status_code = status_code;
    built.content_type = "application/json; charset=utf-8";
    built.body = sql_strdup(payload);
    if (built.body == NULL) {
        sql_set_error(error, 0, 0, "failed to allocate HTTP response body");
        return 0;
    }

    built.body_length = strlen(built.body);
    *response = built;
    return 1;
}

static char *build_error_payload(const char *message, SqlError *error) {
    const char *text = message != NULL ? message : "unknown request error";
    char *quoted = sql_json_quote(text, error);
    char *payload;
    size_t capacity;

    if (quoted == NULL) {
        return NULL;
    }

    capacity = strlen(quoted) + strlen("{\"ok\":false,\"error\":{\"message\":}}") + 1U;
    payload = (char *) malloc(capacity);
    if (payload == NULL) {
        free(quoted);
        sql_set_error(error, 0, 0, "failed to allocate JSON error payload");
        return NULL;
    }

    (void) snprintf(payload, capacity, "{\"ok\":false,\"error\":{\"message\":%s}}", quoted);
    free(quoted);
    return payload;
}

static int set_json_error_response(HttpResponse *response, int status_code, const char *message, SqlError *error) {
    char *payload = build_error_payload(message, error);
    int ok;

    if (payload == NULL) {
        return 0;
    }

    ok = set_json_response(response, status_code, payload, error);
    free(payload);
    return ok;
}

static int execute_sql_work(void *work_ctx, SqlError *error) {
    RequestExecutionContext *context = work_ctx;

    if (context == NULL) {
        sql_set_error(error, 0, 0, "request execution context must not be null");
        return 0;
    }

    context->result = sql_service_execute(context->sql_service, context->sql_text, error);
    return context->result.payload != NULL;
}

RequestHandler *request_handler_create(SqlService *sql_service, DbGuard *db_guard, SqlError *error) {
    RequestHandler *handler;

    if (sql_service == NULL) {
        sql_set_error(error, 0, 0, "request_handler_create requires a SQL service");
        return NULL;
    }
    if (db_guard == NULL) {
        sql_set_error(error, 0, 0, "request_handler_create requires a DB guard");
        return NULL;
    }

    handler = (RequestHandler *) malloc(sizeof(*handler));
    if (handler == NULL) {
        sql_set_error(error, 0, 0, "failed to allocate request handler");
        return NULL;
    }

    handler->sql_service = sql_service;
    handler->db_guard = db_guard;
    return handler;
}

int request_handler_handle(RequestHandler *handler,
                           const HttpRequest *request,
                           HttpResponse *response,
                           SqlError *error) {
    SqlOperationKind operation = SQL_OPERATION_UNKNOWN;
    SqlError classify_error = {0};
    SqlError execution_error = {0};
    RequestExecutionContext execution = {0};
    int executed = 0;
    int response_ok = 0;

    if (handler == NULL || request == NULL || response == NULL) {
        sql_set_error(error, 0, 0, "request_handler_handle received invalid arguments");
        return 0;
    }

    if (handler->sql_service == NULL) {
        return set_json_error_response(response, 500, "SQL service is not configured.", error);
    }
    if (handler->db_guard == NULL) {
        return set_json_error_response(response, 500, "DB guard is not configured.", error);
    }

    if (strcmp(request->method, "POST") != 0) {
        return set_json_error_response(response, 405, "Only POST requests are supported.", error);
    }

    if (request->path[0] != '/') {
        return set_json_error_response(response, 400, "Request path must start with '/'.", error);
    }

    if (request->body == NULL || request->body_length == 0U || is_blank_text(request->body, request->body_length)) {
        return set_json_error_response(response, 400, "Request body must contain SQL text.", error);
    }

    if (!sql_service_classify_operation(request->body, &operation, &classify_error)) {
        return set_json_error_response(response, 400, classify_error.message, error);
    }

    execution.sql_service = handler->sql_service;
    execution.sql_text = request->body;

    if (operation == SQL_OPERATION_READ) {
        executed = db_guard_execute_read(handler->db_guard, execute_sql_work, &execution, &execution_error);
    } else if (operation == SQL_OPERATION_WRITE) {
        executed = db_guard_execute_write(handler->db_guard, execute_sql_work, &execution, &execution_error);
    } else {
        return set_json_error_response(response, 400, "Unsupported SQL operation.", error);
    }

    if (execution.result.payload != NULL) {
        response_ok = set_json_response(response, execution.result.ok ? 200 : 400, execution.result.payload, error);
        sql_service_result_destroy(&execution.result);
        return response_ok;
    }

    if (!executed) {
        return set_json_error_response(response,
                                       500,
                                       execution_error.message[0] != '\0' ? execution_error.message
                                                                           : "Failed to execute SQL request.",
                                       error);
    }

    return set_json_error_response(response, 500, "SQL request completed without a response payload.", error);
}

void request_handler_destroy(RequestHandler *handler) {
    free(handler);
}
