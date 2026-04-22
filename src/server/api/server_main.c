#include "server.h"
#include "socket_server_internal.h"

#include <stdlib.h>

static int server_config_validate(const ServerConfig *config, SqlError *error) {
    if (config == NULL) {
        sql_set_error(error, 0, 0, "server config must not be null");
        return 0;
    }
    if (memchr(config->host, '\0', sizeof(config->host)) == NULL) {
        sql_set_error(error, 0, 0, "server host must be null terminated");
        return 0;
    }
    if (config->port <= 0 || config->port > 65535) {
        sql_set_error(error, 0, 0, "server port must be between 1 and 65535");
        return 0;
    }
    if (config->backlog <= 0) {
        sql_set_error(error, 0, 0, "server backlog must be positive");
        return 0;
    }
    if (config->worker_count <= 0) {
        sql_set_error(error, 0, 0, "server worker_count must be positive");
        return 0;
    }
    if (config->queue_capacity <= 0) {
        sql_set_error(error, 0, 0, "server queue_capacity must be positive");
        return 0;
    }
    if (config->client_read_timeout_ms < 0) {
        sql_set_error(error, 0, 0, "server client_read_timeout_ms must not be negative");
        return 0;
    }
    return 1;
}

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
 */
DbServer *server_create(const ServerConfig *config, SqlError *error) {
    DbServer *server;

    if (!server_config_validate(config, error)) {
        return NULL;
    }

    server = (DbServer *) calloc(1U, sizeof(*server));
    if (server == NULL) {
        sql_set_error(error, 0, 0, "failed to allocate server runtime");
        return NULL;
    }

    server->listen_fd = -1;
    server->is_running = 0;
    server->config = *config;
    server->thread_pool = thread_pool_create(config->worker_count, config->queue_capacity, error);
    if (server->thread_pool == NULL) {
        free(server);
        return NULL;
    }
    return server;
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
 * - `.ready` 전 단계에서는 accept 루프와 연결 수명주기 골격만 동작한다.
 * - 스레드풀 준비가 끝나면 요청을 작업 큐에 전달하는 연결이 추가된다.
 *
 */
int server_start(DbServer *server, SqlError *error) {
    int listen_fd;

    if (server == NULL) {
        sql_set_error(error, 0, 0, "server must not be null");
        return 0;
    }
    if (server->is_running) {
        sql_set_error(error, 0, 0, "server is already running");
        return 0;
    }
    if (server->thread_pool == NULL) {
        sql_set_error(error, 0, 0, "server thread pool is not initialized");
        return 0;
    }

    listen_fd = server_socket_open(&server->config, error);
    if (listen_fd < 0) {
        return 0;
    }

    server->listen_fd = listen_fd;
    server->is_running = 1;

    if (!thread_pool_start(server->thread_pool, error)) {
        server->is_running = 0;
        server_socket_close(&server->listen_fd);
        return 0;
    }

    if (!server_socket_accept_loop(server, error)) {
        server_stop(server);
        return 0;
    }

    server_stop(server);
    return 1;
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
 */
void server_stop(DbServer *server) {
    if (server == NULL) {
        return;
    }

    server->is_running = 0;
    server_socket_close(&server->listen_fd);
    thread_pool_stop(server->thread_pool);
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
 */
void server_destroy(DbServer *server) {
    if (server == NULL) {
        return;
    }

    server_stop(server);
    thread_pool_destroy(server->thread_pool);
    free(server);
}
