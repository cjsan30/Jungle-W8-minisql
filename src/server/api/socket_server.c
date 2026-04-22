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

static int server_ascii_case_equals(const char *left, size_t left_length, const char *right) {
    size_t index;

    if (strlen(right) != left_length) {
        return 0;
    }

    for (index = 0; index < left_length; ++index) {
        unsigned char left_char = (unsigned char) left[index];
        unsigned char right_char = (unsigned char) right[index];

        if (left_char >= 'A' && left_char <= 'Z') {
            left_char = (unsigned char) (left_char - 'A' + 'a');
        }
        if (right_char >= 'A' && right_char <= 'Z') {
            right_char = (unsigned char) (right_char - 'A' + 'a');
        }
        if (left_char != right_char) {
            return 0;
        }
    }

    return 1;
}

static const char *server_find_request_terminator(const char *buffer,
                                                  size_t length,
                                                  size_t *terminator_length) {
    size_t index;

    if (buffer == NULL || terminator_length == NULL || length < 2U) {
        return NULL;
    }

    for (index = 0; index + 1U < length; ++index) {
        if (buffer[index] == '\n' && buffer[index + 1U] == '\n') {
            *terminator_length = 2U;
            return buffer + index;
        }
        if (index + 3U < length &&
            buffer[index] == '\r' &&
            buffer[index + 1U] == '\n' &&
            buffer[index + 2U] == '\r' &&
            buffer[index + 3U] == '\n') {
            *terminator_length = 4U;
            return buffer + index;
        }
    }

    return NULL;
}

static int server_parse_content_length(const char *buffer,
                                       size_t header_length,
                                       size_t *content_length,
                                       SqlError *error) {
    const char *cursor = buffer;
    const char *headers_end = buffer + header_length;

    *content_length = 0U;
    while (cursor < headers_end) {
        const char *line_end = cursor;
        const char *colon;
        const char *value_start;
        size_t name_length;
        size_t parsed_length = 0U;
        int saw_digit = 0;

        while (line_end < headers_end && *line_end != '\r' && *line_end != '\n') {
            line_end++;
        }

        if (line_end == cursor) {
            return 1;
        }

        colon = memchr(cursor, ':', (size_t) (line_end - cursor));
        if (colon != NULL) {
            name_length = (size_t) (colon - cursor);
            if (server_ascii_case_equals(cursor, name_length, "Content-Length")) {
                value_start = colon + 1;
                while (value_start < line_end &&
                       (*value_start == ' ' || *value_start == '\t')) {
                    value_start++;
                }

                while (value_start < line_end) {
                    if (*value_start >= '0' && *value_start <= '9') {
                        saw_digit = 1;
                        parsed_length = parsed_length * 10U + (size_t) (*value_start - '0');
                    } else if (*value_start == ' ' || *value_start == '\t') {
                        const char *trailing = value_start + 1;
                        while (trailing < line_end) {
                            if (*trailing != ' ' && *trailing != '\t') {
                                sql_set_error(error, 0, 0, "Content-Length header is invalid");
                                return 0;
                            }
                            trailing++;
                        }
                        break;
                    } else {
                        sql_set_error(error, 0, 0, "Content-Length header is invalid");
                        return 0;
                    }
                    value_start++;
                }

                if (!saw_digit) {
                    sql_set_error(error, 0, 0, "Content-Length header is missing a numeric value");
                    return 0;
                }

                *content_length = parsed_length;
                return 1;
            }
        }

        cursor = line_end;
        while (cursor < headers_end && (*cursor == '\r' || *cursor == '\n')) {
            cursor++;
        }
    }

    return 1;
}

static ServerRequestReadStatus server_socket_read_request(int client_fd,
                                                          char **out_buffer,
                                                          size_t *out_length,
                                                          SqlError *error) {
    char *buffer;
    size_t capacity;
    size_t length;
    int headers_complete = 0;
    size_t header_length = 0U;
    size_t content_length = 0U;

    buffer = (char *) malloc(SERVER_REQUEST_INITIAL_CAPACITY + 1U);
    if (buffer == NULL) {
        sql_set_error(error, 0, 0, "failed to allocate request buffer");
        return SERVER_REQUEST_READ_ERROR;
    }

    capacity = SERVER_REQUEST_INITIAL_CAPACITY;
    length = 0U;

    while (1) {
        ssize_t bytes_read;

        if (headers_complete &&
            header_length + content_length > SERVER_REQUEST_MAX_CAPACITY) {
            free(buffer);
            sql_set_error(error, 0, 0, "request exceeded the maximum supported size");
            return SERVER_REQUEST_READ_TOO_LARGE;
        }
        if (length == capacity) {
            char *resized_buffer;
            size_t next_capacity;

            if (capacity >= SERVER_REQUEST_MAX_CAPACITY) {
                free(buffer);
                sql_set_error(error, 0, 0, "request exceeded the maximum supported size");
                return SERVER_REQUEST_READ_TOO_LARGE;
            }

            next_capacity = capacity * 2U;
            if (next_capacity > SERVER_REQUEST_MAX_CAPACITY) {
                next_capacity = SERVER_REQUEST_MAX_CAPACITY;
            }

            resized_buffer = (char *) realloc(buffer, next_capacity + 1U);
            if (resized_buffer == NULL) {
                free(buffer);
                sql_set_error(error, 0, 0, "failed to grow request buffer");
                return SERVER_REQUEST_READ_ERROR;
            }

            buffer = resized_buffer;
            capacity = next_capacity;
        }

        bytes_read = recv(client_fd, buffer + length, capacity - length, 0);
        if (bytes_read > 0) {
            size_t terminator_length = 0U;
            const char *header_terminator;

            length += (size_t) bytes_read;
            if (!headers_complete) {
                header_terminator = server_find_request_terminator(buffer, length, &terminator_length);
                if (header_terminator != NULL) {
                    header_length = (size_t) (header_terminator - buffer) + terminator_length;
                    if (!server_parse_content_length(buffer, header_length, &content_length, error)) {
                        free(buffer);
                        return SERVER_REQUEST_READ_ERROR;
                    }
                    headers_complete = 1;
                }
            }
            if (headers_complete && length >= header_length + content_length) {
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
        sql_set_error(error, 0, 0, "failed to receive request bytes");
        return SERVER_REQUEST_READ_ERROR;
    }

    if (length == 0U) {
        free(buffer);
        return SERVER_REQUEST_READ_EMPTY;
    }
    if (!headers_complete) {
        free(buffer);
        sql_set_error(error, 0, 0, "HTTP request headers are incomplete");
        return SERVER_REQUEST_READ_ERROR;
    }
    if (length < header_length + content_length) {
        free(buffer);
        sql_set_error(error, 0, 0, "HTTP request body is incomplete");
        return SERVER_REQUEST_READ_ERROR;
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

static int server_socket_process_request(ServerJobData *server_job_data, SqlError *error) {
    HttpRequest request = {0};
    HttpResponse response = {0};
    char *serialized = NULL;
    int ok = 0;

    if (server_job_data == NULL || server_job_data->request_handler == NULL) {
        sql_set_error(error, 0, 0, "server request job is not initialized");
        return 0;
    }
    if (!http_parse_request(server_job_data->raw_request, &request, error)) {
        return server_socket_send_plain_response(server_job_data->client_fd,
                                                 "400 Bad Request",
                                                 error->message[0] != '\0' ? error->message
                                                                           : "failed to parse HTTP request\n");
    }
    if (!request_handler_handle(server_job_data->request_handler, &request, &response, error)) {
        ok = server_socket_send_plain_response(server_job_data->client_fd,
                                               "500 Internal Server Error",
                                               error->message[0] != '\0' ? error->message
                                                                         : "failed to handle HTTP request\n");
        http_request_destroy(&request);
        return ok;
    }

    serialized = http_serialize_response(&response, error);
    if (serialized == NULL) {
        ok = server_socket_send_plain_response(server_job_data->client_fd,
                                               "500 Internal Server Error",
                                               error->message[0] != '\0' ? error->message
                                                                         : "failed to serialize HTTP response\n");
        http_response_destroy(&response);
        http_request_destroy(&request);
        return ok;
    }

    ok = server_socket_write_all(server_job_data->client_fd, serialized, strlen(serialized));
    free(serialized);
    http_response_destroy(&response);
    http_request_destroy(&request);
    return ok;
}

static void server_socket_execute_request_job(void *job_data) {
    ServerJobData *server_job_data = job_data;
    SqlError error = {0};

    if (server_job_data == NULL) {
        return;
    }

    if (!server_socket_process_request(server_job_data, &error)) {
        (void) server_socket_send_plain_response(
            server_job_data->client_fd,
            "500 Internal Server Error",
            error.message[0] != '\0' ? error.message : "request processing failed\n");
    }
}

static ServerJobData *server_socket_create_job_data(int client_fd,
                                                    char *request_buffer,
                                                    size_t request_length,
                                                    RequestHandler *request_handler,
                                                    SqlError *error) {
    ServerJobData *job_data;

    job_data = (ServerJobData *) calloc(1U, sizeof(*job_data));
    if (job_data == NULL) {
        free(request_buffer);
        close(client_fd);
        sql_set_error(error, 0, 0, "failed to allocate server job payload");
        return NULL;
    }

    server_job_data_init(job_data, client_fd, request_buffer, request_length, request_handler);
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

    if (server == NULL || server->thread_pool == NULL || server->request_handler == NULL) {
        sql_set_error(error, 0, 0, "server thread pool is not ready");
        return 0;
    }

    job_data = server_socket_create_job_data(
        client_fd,
        request_buffer,
        request_length,
        server->request_handler,
        error);
    if (job_data == NULL) {
        return 0;
    }

    job = server_job_build(server_socket_execute_request_job, server_job_cleanup, job_data);
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

    read_status = server_socket_read_request(client_fd, &request_buffer, &request_length, error);
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
            error != NULL && error->message[0] != '\0' ? error->message : "failed to read request\n");
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
