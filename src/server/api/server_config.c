#include "server_config.h"

/*
 * 기능:
 * - API 서버 실행에 필요한 기본 설정값을 채운다.
 *
 * 반환값:
 * - 없음
 *
 * 흐름:
 * - NULL 입력이면 바로 종료한다.
 * - 구조체를 0으로 초기화한다.
 * - host, port, backlog, worker 수, queue 크기, timeout 기본값을 채운다.
 */
void server_config_set_defaults(ServerConfig *config) {
    if (config == NULL) {
        return;
    }

    memset(config, 0, sizeof(*config));
    strncpy(config->host, "0.0.0.0", sizeof(config->host) - 1);
    strncpy(config->db_root, "./data", sizeof(config->db_root) - 1);
    config->port = 8080;
    config->backlog = 128;
    config->worker_count = 4;
    config->queue_capacity = 128;
    config->client_read_timeout_ms = 3000;
}
