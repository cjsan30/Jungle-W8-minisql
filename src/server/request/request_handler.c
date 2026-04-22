#include "request_handler.h"
#include <ctype.h>

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

static int set_text_response(HttpResponse *response, int status_code, const char *body, SqlError *error) {
    HttpResponse built = {0};
    const char *payload = body != NULL ? body : "";

    built.status_code = status_code;
    built.content_type = "text/plain; charset=utf-8";
    built.body = sql_strdup(payload);
    if (built.body == NULL) {
        sql_set_error(error, 0, 0, "failed to allocate HTTP response body");
        return 0;
    }

    built.body_length = strlen(built.body);
    *response = built;
    return 1;
}

/*
 * 기능:
 * - 요청 처리기 객체를 생성한다.
 *
 * 반환값:
 * - 성공: `RequestHandler *`
 * - 실패: `NULL`, error에 원인 기록
 *
 * 흐름:
 * - sql_service와 db_guard 의존성을 연결한다.
 * - 요청 처리에 필요한 상태를 초기화한다.
 *
 * 현재 상태:
 * - 의존성 연결과 최소 유효성 검사까지 담당한다.
 */
RequestHandler *request_handler_create(SqlService *sql_service, DbGuard *db_guard, SqlError *error) {
    RequestHandler *handler;

    if (sql_service == NULL) {
        sql_set_error(error, 0, 0, "request_handler_create requires a SQL service");
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

/*
 * 기능:
 * - HTTP 요청 1건을 처리해 응답 객체를 만든다.
 *
 * 반환값:
 * - 성공: 1
 * - 실패: 0, error에 원인 기록
 *
 * 흐름:
 * - 요청 유효성을 검사한다.
 * - SQL 실행 계층으로 전달한다.
 * - 실행 결과를 HTTP 응답으로 변환한다.
 *
 * 현재 상태:
 * - 요청 검증과 표준 에러 응답 생성까지 구현한다.
 * - DB 실행 연결은 `src/server/concurrency/.ready` 이후에 붙인다.
 */
int request_handler_handle(RequestHandler *handler,
                           const HttpRequest *request,
                           HttpResponse *response,
                           SqlError *error) {
    SqlOperationKind operation = SQL_OPERATION_UNKNOWN;
    SqlError classify_error = {0};

    if (handler == NULL || request == NULL || response == NULL) {
        sql_set_error(error, 0, 0, "request_handler_handle received invalid arguments");
        return 0;
    }

    if (handler->sql_service == NULL) {
        return set_text_response(response, 500, "SQL service is not configured.\n", error);
    }

    if (strcmp(request->method, "POST") != 0) {
        return set_text_response(response, 405, "Only POST requests are supported.\n", error);
    }

    if (request->path[0] != '/') {
        return set_text_response(response, 400, "Request path must start with '/'.\n", error);
    }

    if (request->body == NULL || request->body_length == 0U || is_blank_text(request->body, request->body_length)) {
        return set_text_response(response, 400, "Request body must contain SQL text.\n", error);
    }

    if (!sql_service_classify_operation(request->body, &operation, &classify_error)) {
        return set_text_response(response, 400, classify_error.message, error);
    }

    (void) operation;
    return set_text_response(response, 503, "DB execution bridge is not integrated yet.\n", error);
}

/*
 * 기능:
 * - 요청 처리기 객체를 해제한다.
 *
 * 반환값:
 * - 없음
 *
 * 흐름:
 * - 내부 상태가 있으면 정리한다.
 * - 마지막에 처리기 구조체를 해제한다.
 *
 * 현재 상태:
 * - 처리기 자신만 해제하고 외부 의존성 소유권은 건드리지 않는다.
 */
void request_handler_destroy(RequestHandler *handler) {
    free(handler);
}
