#include "http_protocol.h"

static const char *find_request_line_end(const char *raw_request) {
    const char *crlf;
    const char *lf;

    crlf = strstr(raw_request, "\r\n");
    lf = strchr(raw_request, '\n');
    if (crlf != NULL && (lf == NULL || crlf < lf)) {
        return crlf;
    }
    return lf;
}

static const char *find_body_start(const char *raw_request) {
    const char *crlf;
    const char *lf;

    crlf = strstr(raw_request, "\r\n\r\n");
    lf = strstr(raw_request, "\n\n");
    if (crlf != NULL && (lf == NULL || crlf < lf)) {
        return crlf + 4;
    }
    if (lf != NULL) {
        return lf + 2;
    }
    return NULL;
}

static int copy_request_token(char *dest,
                              size_t dest_size,
                              const char *start,
                              size_t length,
                              const char *label,
                              SqlError *error) {
    if (length == 0U) {
        sql_set_error(error, 0, 0, "%s must not be empty", label);
        return 0;
    }
    if (length >= dest_size) {
        sql_set_error(error, 0, 0, "%s is too long", label);
        return 0;
    }

    memcpy(dest, start, length);
    dest[length] = '\0';
    return 1;
}

static const char *http_reason_phrase(int status_code) {
    switch (status_code) {
        case 200:
            return "OK";
        case 400:
            return "Bad Request";
        case 404:
            return "Not Found";
        case 405:
            return "Method Not Allowed";
        case 500:
            return "Internal Server Error";
        case 503:
            return "Service Unavailable";
        default:
            return "Unknown";
    }
}

/*
 * 기능:
 * - raw HTTP 요청 문자열을 구조화된 요청 객체로 파싱한다.
 *
 * 반환값:
 * - 성공: 1
 * - 실패: 0, error에 원인 기록
 *
 * 흐름:
 * - 요청 라인에서 method/path를 읽는다.
 * - 헤더와 body 경계를 찾는다.
 * - body를 복사해 `HttpRequest`에 채운다.
 *
 * 현재 상태:
 * - 요청 라인과 body 추출까지 담당한다.
 */
int http_parse_request(const char *raw_request, HttpRequest *request, SqlError *error) {
    HttpRequest parsed = {0};
    const char *line_end;
    const char *method_end;
    const char *path_start;
    const char *path_end;
    const char *body_start;
    size_t body_length;

    if (raw_request == NULL || request == NULL) {
        sql_set_error(error, 0, 0, "http_parse_request received invalid arguments");
        return 0;
    }
    if (raw_request[0] == '\0') {
        sql_set_error(error, 0, 0, "raw HTTP request must not be empty");
        return 0;
    }

    line_end = find_request_line_end(raw_request);
    if (line_end == NULL) {
        sql_set_error(error, 0, 0, "HTTP request line must end with newline");
        return 0;
    }

    method_end = raw_request;
    while (method_end < line_end && *method_end != ' ' && *method_end != '\t') {
        method_end++;
    }
    if (!copy_request_token(parsed.method,
                            sizeof(parsed.method),
                            raw_request,
                            (size_t) (method_end - raw_request),
                            "HTTP method",
                            error)) {
        return 0;
    }

    path_start = method_end;
    while (path_start < line_end && (*path_start == ' ' || *path_start == '\t')) {
        path_start++;
    }
    path_end = path_start;
    while (path_end < line_end && *path_end != ' ' && *path_end != '\t') {
        path_end++;
    }
    if (!copy_request_token(parsed.path,
                            sizeof(parsed.path),
                            path_start,
                            (size_t) (path_end - path_start),
                            "HTTP path",
                            error)) {
        return 0;
    }

    body_start = find_body_start(raw_request);
    if (body_start != NULL && body_start[0] != '\0') {
        body_length = strlen(body_start);
        parsed.body = (char *) malloc(body_length + 1U);
        if (parsed.body == NULL) {
            sql_set_error(error, 0, 0, "failed to allocate HTTP request body");
            return 0;
        }

        memcpy(parsed.body, body_start, body_length + 1U);
        parsed.body_length = body_length;
    }

    *request = parsed;
    return 1;
}

/*
 * 기능:
 * - 구조화된 HTTP 응답을 문자열로 직렬화한다.
 *
 * 반환값:
 * - 성공: 직렬화된 문자열 포인터
 * - 실패: `NULL`, error에 원인 기록
 *
 * 흐름:
 * - 상태 코드와 헤더를 조합한다.
 * - body 길이를 계산한다.
 * - 최종 응답 문자열을 생성한다.
 *
 * 현재 상태:
 * - 상태 코드/헤더/body를 하나의 HTTP/1.1 문자열로 직렬화한다.
 */
char *http_serialize_response(const HttpResponse *response, SqlError *error) {
    static const char *format =
            "HTTP/1.1 %d %s\r\n"
            "Content-Type: %s\r\n"
            "Content-Length: %zu\r\n"
            "Connection: close\r\n"
            "\r\n";
    const char *reason_phrase;
    const char *content_type;
    const char *body;
    size_t body_length;
    int header_length;
    size_t total_length;
    char *serialized;

    if (response == NULL) {
        sql_set_error(error, 0, 0, "http_serialize_response received invalid arguments");
        return NULL;
    }

    if (response->body == NULL && response->body_length != 0U) {
        sql_set_error(error, 0, 0, "HTTP response body pointer/length are inconsistent");
        return NULL;
    }

    reason_phrase = http_reason_phrase(response->status_code);
    content_type = response->content_type != NULL ? response->content_type : "text/plain; charset=utf-8";
    body = response->body != NULL ? response->body : "";
    body_length = response->body != NULL && response->body_length == 0U ? strlen(response->body)
                                                                         : response->body_length;

    header_length = snprintf(NULL,
                             0,
                             format,
                             response->status_code,
                             reason_phrase,
                             content_type,
                             body_length);
    if (header_length < 0) {
        sql_set_error(error, 0, 0, "failed to calculate HTTP response size");
        return NULL;
    }

    total_length = (size_t) header_length + body_length;
    serialized = (char *) malloc(total_length + 1U);
    if (serialized == NULL) {
        sql_set_error(error, 0, 0, "failed to allocate HTTP response buffer");
        return NULL;
    }

    if (snprintf(serialized,
                 (size_t) header_length + 1U,
                 format,
                 response->status_code,
                 reason_phrase,
                 content_type,
                 body_length) < 0) {
        free(serialized);
        sql_set_error(error, 0, 0, "failed to write HTTP response headers");
        return NULL;
    }

    if (body_length > 0U) {
        memcpy(serialized + (size_t) header_length, body, body_length);
    }
    serialized[total_length] = '\0';
    return serialized;
}

/*
 * 기능:
 * - `HttpRequest` 내부 동적 메모리를 해제한다.
 *
 * 반환값:
 * - 없음
 *
 * 흐름:
 * - body를 해제하고 길이를 초기화한다.
 */
void http_request_destroy(HttpRequest *request) {
    if (request == NULL) {
        return;
    }

    free(request->body);
    request->method[0] = '\0';
    request->path[0] = '\0';
    request->body = NULL;
    request->body_length = 0U;
}

/*
 * 기능:
 * - `HttpResponse` 내부 동적 메모리를 해제한다.
 *
 * 반환값:
 * - 없음
 *
 * 흐름:
 * - body를 해제하고 길이를 초기화한다.
 */
void http_response_destroy(HttpResponse *response) {
    if (response == NULL) {
        return;
    }

    free(response->body);
    response->status_code = 0;
    response->content_type = NULL;
    response->body = NULL;
    response->body_length = 0U;
}
