#include "server.h"

/*
 * 기능:
 * - 서버 런타임 객체를 생성한다.
 *
 * 반환값:
 * - 성공: `DbServer *`
 * - 실패: `NULL`, error에 원인 기록
 *
 * 흐름:
 * - 설정값을 검증한다.
 * - 서버 구조체와 내부 의존성을 준비한다.
 * - 이후 `server_start`에서 실제 실행을 시작한다.
 *
 * 현재 상태:
 * - 서버 생성 흐름을 위한 stub 함수다.
 */
DbServer *server_create(const ServerConfig *config, SqlError *error) {
    (void) config;
    sql_set_error(error, 0, 0, "server_create stub: API 서버 초기화 로직이 아직 구현되지 않았습니다");
    return NULL;
}

/*
 * 기능:
 * - 서버를 시작하고 요청 수신 루프를 돈다.
 *
 * 반환값:
 * - 성공: 1
 * - 실패: 0, error에 원인 기록
 *
 * 흐름:
 * - listen 소켓을 준비한다.
 * - thread pool을 시작한다.
 * - 클라이언트 요청을 받아 작업 큐에 전달한다.
 *
 * 현재 상태:
 * - 실행 흐름을 위한 stub 함수다.
 */
int server_start(DbServer *server, SqlError *error) {
    (void) server;
    sql_set_error(error, 0, 0, "server_start stub: listen 소켓/accept 루프가 아직 구현되지 않았습니다");
    return 0;
}

/*
 * 기능:
 * - 실행 중인 서버를 중지한다.
 *
 * 반환값:
 * - 없음
 *
 * 흐름:
 * - 수신 루프를 멈춘다.
 * - listen fd와 worker 종료를 유도한다.
 *
 * 현재 상태:
 * - 종료 지점을 위한 stub 함수다.
 */
void server_stop(DbServer *server) {
    (void) server;
}

/*
 * 기능:
 * - 서버 객체와 내부 자원을 해제한다.
 *
 * 반환값:
 * - 없음
 *
 * 흐름:
 * - 내부 자원이 남아 있으면 먼저 정리한다.
 * - 마지막에 서버 구조체 자체를 해제한다.
 *
 * 현재 상태:
 * - 정리 지점을 위한 stub 함수다.
 */
void server_destroy(DbServer *server) {
    (void) server;
}
