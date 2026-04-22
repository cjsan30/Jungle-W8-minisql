#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "server_config.h"

static void test_server_config_sets_safe_defaults(void) {
    ServerConfig config;

    memset(&config, 0x7f, sizeof(config));
    server_config_set_defaults(&config);

    assert(strcmp(config.host, "127.0.0.1") == 0);
    assert(config.port == 8080);
    assert(config.backlog == 128);
    assert(config.worker_count == 4);
    assert(config.queue_capacity == 128);
    assert(config.client_read_timeout_ms == 3000);
}

static void test_server_config_accepts_null_pointer(void) {
    server_config_set_defaults(NULL);
}

int main(void) {
    test_server_config_sets_safe_defaults();
    test_server_config_accepts_null_pointer();
    printf("server config unit tests passed\n");
    return 0;
}
