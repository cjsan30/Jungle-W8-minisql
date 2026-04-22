#include "request_handler.h"

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
 * - 요청 처리기 생성을 위한 stub 함수다.
 */
RequestHandler *request_handler_create(SqlService *sql_service, DbGuard *db_guard, SqlError *error) {
    (void) sql_service;
    (void) db_guard;
    sql_set_error(error, 0, 0, "request_handler_create stub: 요청 처리기 초기화 로직이 아직 구현되지 않았습니다");
    return NULL;
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
 * - 요청 처리 오케스트레이션을 위한 stub 함수다.
 */
int request_handler_handle(RequestHandler *handler,
                           const HttpRequest *request,
                           HttpResponse *response,
                           SqlError *error) {
    (void) handler;
    (void) request;
    (void) response;
    sql_set_error(error, 0, 0, "request_handler_handle stub: 요청 -> SQL 실행 연결 로직이 아직 구현되지 않았습니다");
    return 0;
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
 * - 해제 지점을 위한 stub 함수다.
 */
void request_handler_destroy(RequestHandler *handler) {
    (void) handler;
}
