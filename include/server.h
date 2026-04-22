#ifndef SERVER_H
#define SERVER_H

#include "common.h"
#include "server_config.h"
#include "thread_pool.h"

typedef struct {
    int listen_fd;
    int is_running;
    ServerConfig config;
    ThreadPool *thread_pool;
} DbServer;

/*
 * 기능:
 * - DB API 서버 전체를 생성한다.
 * - 소켓 서버, 스레드풀, 요청 처리기를 붙일 최상위 런타임 객체다.
 *
 * 반환값:
 * - 성공: 초기화된 DbServer 포인터
 * - 실패: NULL, error에 실패 원인 기록
 *
 * 흐름:
 * - 설정값을 검증한다.
 * - 서버 상태 구조체를 할당한다.
 * - 이후 server_start에서 실제 소켓/워커를 띄운다.
 */
DbServer *server_create(const ServerConfig *config, SqlError *error);

/*
 * 기능:
 * - API 서버를 시작한다.
 * - listen 소켓을 열고 클라이언트 요청을 받아 작업 큐로 전달하는 진입점이다.
 *
 * 반환값:
 * - 성공: 1
 * - 실패: 0, error에 실패 원인 기록
 *
 * 흐름:
 * - listen 소켓을 준비한다.
 * - `.ready` 전 단계에서는 accept 루프와 연결 수명주기 골격만 동작한다.
 * - 스레드풀 준비가 끝나면 요청을 worker에게 넘기는 연결이 추가된다.
 */
int server_start(DbServer *server, SqlError *error);

/*
 * 기능:
 * - 실행 중인 서버를 정상 종료한다.
 * - accept 루프와 스레드풀 종료 절차를 담당한다.
 *
 * 반환값:
 * - 없음
 *
 * 흐름:
 * - 서버 상태를 중지로 전환한다.
 * - listen fd와 worker 자원을 정리한다.
 */
void server_stop(DbServer *server);

/*
 * 기능:
 * - 서버 객체를 메모리에서 해제한다.
 *
 * 반환값:
 * - 없음
 *
 * 흐름:
 * - 내부 자원이 남아 있으면 먼저 정리한다.
 * - 마지막에 서버 구조체 자체를 free한다.
 */
void server_destroy(DbServer *server);

#endif
