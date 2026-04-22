#include "server.h"
#include "server_job.h"
#include "socket_server_internal.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#define SERVER_REQUEST_INITIAL_CAPACITY 1024U
#define SERVER_REQUEST_MAX_CAPACITY 65536U

typedef enum {
    SERVER_REQUEST_READ_OK = 1,
    SERVER_REQUEST_READ_EMPTY = 0,
    SERVER_REQUEST_READ_TOO_LARGE = -1,
    SERVER_REQUEST_READ_ERROR = -2
} ServerRequestReadStatus;

static void server_set_errno_error(SqlError *error, const char *context) {
    sql_set_error(error, 0, 0, "%s: %s", context, strerror(errno));
}

static int server_socket_set_reuseaddr(int listen_fd, SqlError *error) {
    int enabled = 1;

    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled)) < 0) {
        server_set_errno_error(error, "failed to set SO_REUSEADDR");
        return 0;
    }
    return 1;
}

static int server_socket_set_timeout(int fd, int timeout_ms, SqlError *error) {
    struct timeval timeout;

    if (timeout_ms <= 0) {
        return 1;
    }

    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;

    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        server_set_errno_error(error, "failed to set receive timeout");
        return 0;
    }
    if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
        server_set_errno_error(error, "failed to set send timeout");
        return 0;
    }
    return 1;
}

static int server_socket_disable_sigpipe(int fd) {
#ifdef SO_NOSIGPIPE
    int enabled = 1;

    if (setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &enabled, sizeof(enabled)) < 0) {
        return 0;
    }
#else
    (void) fd;
#endif
    return 1;
}

static int server_buffer_has_request_terminator(const char *buffer, size_t length) {
    size_t index;

    if (length < 2U) {
        return 0;
    }

    for (index = 0; index + 1U < length; ++index) {
        if (buffer[index] == '\n' && buffer[index + 1U] == '\n') {
            return 1;
        }
        if (index + 3U < length &&
            buffer[index] == '\r' &&
            buffer[index + 1U] == '\n' &&
            buffer[index + 2U] == '\r' &&
            buffer[index + 3U] == '\n') {
            return 1;
        }
    }

    return 0;
}

static ServerRequestReadStatus server_socket_read_request(int client_fd,
                                                          char **out_buffer,
                                                          size_t *out_length) {
    char *buffer;
    size_t capacity;
    size_t length;

    buffer = (char *) malloc(SERVER_REQUEST_INITIAL_CAPACITY + 1U);
    if (buffer == NULL) {
        return SERVER_REQUEST_READ_ERROR;
    }

    capacity = SERVER_REQUEST_INITIAL_CAPACITY;
    length = 0U;

    while (1) {
        ssize_t bytes_read;

        if (length == capacity) {
            char *resized_buffer;
            size_t next_capacity;

            if (capacity >= SERVER_REQUEST_MAX_CAPACITY) {
                free(buffer);
                return SERVER_REQUEST_READ_TOO_LARGE;
            }

            next_capacity = capacity * 2U;
            if (next_capacity > SERVER_REQUEST_MAX_CAPACITY) {
                next_capacity = SERVER_REQUEST_MAX_CAPACITY;
            }

            resized_buffer = (char *) realloc(buffer, next_capacity + 1U);
            if (resized_buffer == NULL) {
                free(buffer);
                return SERVER_REQUEST_READ_ERROR;
            }

            buffer = resized_buffer;
            capacity = next_capacity;
        }

        bytes_read = recv(client_fd, buffer + length, capacity - length, 0);
        if (bytes_read > 0) {
            length += (size_t) bytes_read;
            if (server_buffer_has_request_terminator(buffer, length)) {
                break;
            }
            continue;
        }
        if (bytes_read == 0) {
            break;
        }
        if (errno == EINTR) {
            continue;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            break;
        }

        free(buffer);
        return SERVER_REQUEST_READ_ERROR;
    }

    if (length == 0U) {
        free(buffer);
        return SERVER_REQUEST_READ_EMPTY;
    }

    buffer[length] = '\0';
    *out_buffer = buffer;
    *out_length = length;
    return SERVER_REQUEST_READ_OK;
}

static int server_socket_write_all(int client_fd, const char *buffer, size_t length) {
    size_t written = 0U;

    while (written < length) {
        ssize_t bytes_sent;

#ifdef MSG_NOSIGNAL
        bytes_sent = send(client_fd, buffer + written, length - written, MSG_NOSIGNAL);
#else
        bytes_sent = send(client_fd, buffer + written, length - written, 0);
#endif
        if (bytes_sent > 0) {
            written += (size_t) bytes_sent;
            continue;
        }
        if (bytes_sent == 0) {
            return 0;
        }
        if (errno == EINTR) {
            continue;
        }
        return 0;
    }

    return 1;
}

static int server_socket_send_plain_response(int client_fd, const char *status, const char *body) {
    char header[256];
    int header_length;
    size_t body_length;

    body_length = strlen(body);
    header_length = snprintf(header,
                             sizeof(header),
                             "HTTP/1.1 %s\r\n"
                             "Content-Type: text/plain; charset=utf-8\r\n"
                             "Content-Length: %zu\r\n"
                             "Connection: close\r\n"
                             "\r\n",
                             status,
                             body_length);
    if (header_length < 0 || (size_t) header_length >= sizeof(header)) {
        return 0;
    }
    if (!server_socket_write_all(client_fd, header, (size_t) header_length)) {
        return 0;
    }
    if (!server_socket_write_all(client_fd, body, body_length)) {
        return 0;
    }
    return 1;
}

static void server_socket_execute_placeholder_job(void *job_data) {
    ServerJobData *server_job_data = job_data;

    if (server_job_data == NULL) {
        return;
    }

    (void) server_socket_send_plain_response(
        server_job_data->client_fd,
        "503 Service Unavailable",
        "request worker is connected, but sql request handling is not wired yet\n");
}

static ServerJobData *server_socket_create_job_data(int client_fd,
                                                    char *request_buffer,
                                                    size_t request_length,
                                                    SqlError *error) {
    ServerJobData *job_data;

    job_data = (ServerJobData *) calloc(1U, sizeof(*job_data));
    if (job_data == NULL) {
        sql_set_error(error, 0, 0, "failed to allocate server job payload");
        return NULL;
    }

    server_job_data_init(job_data, client_fd, request_buffer, request_length);
    if (!server_job_data_validate(job_data, error)) {
        server_job_cleanup(job_data);
        return NULL;
    }

    return job_data;
}

static int server_socket_dispatch_request(DbServer *server,
                                          int client_fd,
                                          char *request_buffer,
                                          size_t request_length,
                                          SqlError *error) {
    ServerJobData *job_data;
    Job job;

    if (server == NULL || server->thread_pool == NULL) {
        sql_set_error(error, 0, 0, "server thread pool is not ready");
        return 0;
    }

    job_data = server_socket_create_job_data(client_fd, request_buffer, request_length, error);
    if (job_data == NULL) {
        return 0;
    }

    job = server_job_build(server_socket_execute_placeholder_job, server_job_cleanup, job_data);
    if (!thread_pool_submit(server->thread_pool, job, error)) {
        return 0;
    }

    return 1;
}

static int server_socket_handle_client(DbServer *server, int client_fd, SqlError *error) {
    char *request_buffer = NULL;
    size_t request_length = 0U;
    ServerRequestReadStatus read_status;

    if (!server_socket_disable_sigpipe(client_fd)) {
        close(client_fd);
        return 1;
    }
    if (!server_socket_set_timeout(client_fd, server->config.client_read_timeout_ms, NULL)) {
        close(client_fd);
        return 1;
    }

    read_status = server_socket_read_request(client_fd, &request_buffer, &request_length);
    if (read_status == SERVER_REQUEST_READ_OK) {
        if (server_socket_dispatch_request(server, client_fd, request_buffer, request_length, error)) {
            return 1;
        }
        return 0;
    } else if (read_status == SERVER_REQUEST_READ_TOO_LARGE) {
        (void) server_socket_send_plain_response(
            client_fd,
            "413 Payload Too Large",
            "request exceeded the temporary API skeleton limit\n");
    } else if (read_status == SERVER_REQUEST_READ_EMPTY) {
        (void) server_socket_send_plain_response(
            client_fd,
            "400 Bad Request",
            "empty request received\n");
    } else {
        (void) server_socket_send_plain_response(
            client_fd,
            "400 Bad Request",
            "failed to read request\n");
    }

    free(request_buffer);
    close(client_fd);
    return 1;
}

int server_socket_open(const ServerConfig *config, SqlError *error) {
    int listen_fd;
    const char *bind_host;
    struct sockaddr_in address;

    if (config == NULL) {
        sql_set_error(error, 0, 0, "server config must not be null");
        return -1;
    }

    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        server_set_errno_error(error, "failed to create listen socket");
        return -1;
    }
    if (!server_socket_set_reuseaddr(listen_fd, error)) {
        close(listen_fd);
        return -1;
    }

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons((uint16_t) config->port);

    bind_host = config->host[0] == '\0' ? "0.0.0.0" : config->host;
    if (inet_pton(AF_INET, bind_host, &address.sin_addr) != 1) {
        close(listen_fd);
        sql_set_error(error, 0, 0, "server host must be a valid IPv4 address");
        return -1;
    }
    if (bind(listen_fd, (struct sockaddr *) &address, sizeof(address)) < 0) {
        server_set_errno_error(error, "failed to bind listen socket");
        close(listen_fd);
        return -1;
    }
    if (listen(listen_fd, config->backlog) < 0) {
        server_set_errno_error(error, "failed to listen on socket");
        close(listen_fd);
        return -1;
    }

    return listen_fd;
}

int server_socket_accept_loop(DbServer *server, SqlError *error) {
    if (server == NULL) {
        sql_set_error(error, 0, 0, "server must not be null");
        return 0;
    }

    while (server->is_running) {
        int client_fd;

        client_fd = accept(server->listen_fd, NULL, NULL);
        if (client_fd < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (!server->is_running || errno == EBADF || errno == EINVAL) {
                return 1;
            }

            server_set_errno_error(error, "failed to accept client");
            return 0;
        }

        if (!server_socket_handle_client(server, client_fd, error)) {
            return 0;
        }
    }

    return 1;
}

void server_socket_close(int *listen_fd) {
    if (listen_fd == NULL || *listen_fd < 0) {
        return;
    }

    close(*listen_fd);
    *listen_fd = -1;
}
