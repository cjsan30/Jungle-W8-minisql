#ifndef SERVER_CONFIG_H
#define SERVER_CONFIG_H

#include "common.h"

#define SERVER_HOST_BUFFER_SIZE 64

typedef struct {
    char host[SERVER_HOST_BUFFER_SIZE];
    char db_root[SQL_PATH_BUFFER_SIZE];
    int port;
    int backlog;
    int worker_count;
    int queue_capacity;
    int client_read_timeout_ms;
} ServerConfig;

/*
 * 기능:
 * - API 서버의 기본 설정값을 초기화한다.
 * - 포트, backlog, 워커 수, 큐 크기 같은 서버 동작 파라미터를 한 곳에서 관리한다.
 *
 * 반환값:
 * - 없음
 *
 * 흐름:
 * - 호출자가 빈 ServerConfig를 넘긴다.
 * - 함수가 안전한 기본값을 채운다.
 * - 이후 main 또는 서버 초기화 코드가 이 값을 기반으로 서버를 띄운다.
 */
void server_config_set_defaults(ServerConfig *config);

#endif
