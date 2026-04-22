#ifndef HTTP_PROTOCOL_H
#define HTTP_PROTOCOL_H

#include "common.h"

typedef struct {
    char method[16];
    char path[128];
    char *body;
    size_t body_length;
} HttpRequest;

typedef struct {
    int status_code;
    const char *content_type;
    char *body;
    size_t body_length;
} HttpResponse;

/*
 * 기능:
 * - 외부 클라이언트가 보낸 원시 요청 문자열을 HttpRequest로 파싱한다.
 * - 최소한 method, path, body를 추출해 이후 request handler가 사용할 수 있게 만든다.
 *
 * 반환값:
 * - 성공: 1
 * - 실패: 0, error에 실패 원인 기록
 *
 * 흐름:
 * - 요청 라인을 읽는다.
 * - 헤더/본문 경계를 찾는다.
 * - method, path, body를 구조체에 채운다.
 */
int http_parse_request(const char *raw_request, HttpRequest *request, SqlError *error);

/*
 * 기능:
 * - HttpResponse를 소켓에 쓸 수 있는 문자열로 직렬화한다.
 *
 * 반환값:
 * - 성공: 직렬화된 문자열 포인터
 * - 실패: NULL, error에 실패 원인 기록
 *
 * 흐름:
 * - 상태 코드와 헤더를 조합한다.
 * - body 길이를 계산한다.
 * - 최종 HTTP 응답 문자열을 생성한다.
 */
char *http_serialize_response(const HttpResponse *response, SqlError *error);

/*
 * 기능:
 * - HttpRequest 내부 동적 메모리를 해제한다.
 *
 * 반환값:
 * - 없음
 */
void http_request_destroy(HttpRequest *request);

/*
 * 기능:
 * - HttpResponse 내부 동적 메모리를 해제한다.
 *
 * 반환값:
 * - 없음
 */
void http_response_destroy(HttpResponse *response);

#endif
