#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "http_protocol.h"

static void test_http_parse_request_extracts_method_path_and_body(void) {
    const char *raw_request =
        "POST /query HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 33\r\n"
        "\r\n"
        "SELECT * FROM users WHERE id = 2;";
    HttpRequest request = {0};
    SqlError error = {0};

    assert(http_parse_request(raw_request, &request, &error));
    assert(strcmp(request.method, "POST") == 0);
    assert(strcmp(request.path, "/query") == 0);
    assert(strcmp(request.body, "SELECT * FROM users WHERE id = 2;") == 0);
    assert(request.body_length == strlen("SELECT * FROM users WHERE id = 2;"));

    http_request_destroy(&request);
}

static void test_http_parse_request_rejects_missing_request_line_newline(void) {
    HttpRequest request = {0};
    SqlError error = {0};

    assert(!http_parse_request("POST /query HTTP/1.1", &request, &error));
    assert(strstr(error.message, "HTTP request line must end with newline") != NULL);
}

static void test_http_parse_request_rejects_empty_path(void) {
    const char *raw_request =
        "POST \r\n"
        "Host: localhost\r\n"
        "\r\n";
    HttpRequest request = {0};
    SqlError error = {0};

    assert(!http_parse_request(raw_request, &request, &error));
    assert(strstr(error.message, "HTTP path must not be empty") != NULL);
}

static void test_http_serialize_response_writes_status_headers_and_body(void) {
    HttpResponse response = {0};
    SqlError error = {0};
    char *serialized;

    response.status_code = 200;
    response.content_type = "application/json; charset=utf-8";
    response.body = sql_strdup("{\"ok\":true}");
    response.body_length = strlen(response.body);

    serialized = http_serialize_response(&response, &error);
    assert(serialized != NULL);
    assert(strstr(serialized, "HTTP/1.1 200 OK\r\n") != NULL);
    assert(strstr(serialized, "Content-Type: application/json; charset=utf-8\r\n") != NULL);
    assert(strstr(serialized, "Content-Length: 11\r\n") != NULL);
    assert(strstr(serialized, "\r\n\r\n{\"ok\":true}") != NULL);

    free(serialized);
    http_response_destroy(&response);
}

int main(void) {
    test_http_parse_request_extracts_method_path_and_body();
    test_http_parse_request_rejects_missing_request_line_newline();
    test_http_parse_request_rejects_empty_path();
    test_http_serialize_response_writes_status_headers_and_body();
    printf("http protocol unit tests passed\n");
    return 0;
}
