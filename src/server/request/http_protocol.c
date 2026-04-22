#include "http_protocol.h"

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
 * - 요청 파싱을 위한 stub 함수다.
 */
int http_parse_request(const char *raw_request, HttpRequest *request, SqlError *error) {
    (void) raw_request;
    (void) request;
    sql_set_error(error, 0, 0, "http_parse_request stub: HTTP 요청 파싱 로직이 아직 구현되지 않았습니다");
    return 0;
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
 * - 응답 직렬화를 위한 stub 함수다.
 */
char *http_serialize_response(const HttpResponse *response, SqlError *error) {
    (void) response;
    sql_set_error(error, 0, 0, "http_serialize_response stub: HTTP 응답 직렬화 로직이 아직 구현되지 않았습니다");
    return NULL;
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
    response->body = NULL;
    response->body_length = 0U;
}
