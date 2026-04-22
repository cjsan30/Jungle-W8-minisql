#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "request_handler.h"

static RequestHandler *create_handler(SqlService **out_service, DbGuard **out_guard) {
    SqlError error = {0};
    SqlService *service = sql_service_create("./data", &error);
    DbGuard *guard;
    RequestHandler *handler;

    assert(service != NULL);
    guard = db_guard_create(service->db_context, &error);
    assert(guard != NULL);
    handler = request_handler_create(service, guard, &error);
    assert(handler != NULL);

    *out_service = service;
    *out_guard = guard;
    return handler;
}

static void destroy_handler(RequestHandler *handler, SqlService *service, DbGuard *guard) {
    request_handler_destroy(handler);
    db_guard_destroy(guard);
    sql_service_destroy(service);
}

static void test_request_handler_returns_200_for_valid_select(void) {
    SqlService *service = NULL;
    DbGuard *guard = NULL;
    RequestHandler *handler = create_handler(&service, &guard);
    SqlError error = {0};
    HttpRequest request = {0};
    HttpResponse response = {0};

    strncpy(request.method, "POST", sizeof(request.method) - 1);
    strncpy(request.path, "/query", sizeof(request.path) - 1);
    request.body = sql_strdup("SELECT * FROM users WHERE id = 2;");
    request.body_length = strlen(request.body);

    assert(request_handler_handle(handler, &request, &response, &error));
    assert(response.status_code == 200);
    assert(strcmp(response.content_type, "application/json; charset=utf-8") == 0);
    assert(strcmp(response.body,
                  "{\"ok\":true,\"type\":\"select\",\"table\":\"users\",\"rows\":[{\"id\":2,\"name\":\"Bob\",\"age\":22}]}")
           == 0);

    http_response_destroy(&response);
    http_request_destroy(&request);
    destroy_handler(handler, service, guard);
}

static void test_request_handler_rejects_non_post_method(void) {
    SqlService *service = NULL;
    DbGuard *guard = NULL;
    RequestHandler *handler = create_handler(&service, &guard);
    SqlError error = {0};
    HttpRequest request = {0};
    HttpResponse response = {0};

    strncpy(request.method, "GET", sizeof(request.method) - 1);
    strncpy(request.path, "/query", sizeof(request.path) - 1);
    request.body = sql_strdup("SELECT * FROM users;");
    request.body_length = strlen(request.body);

    assert(request_handler_handle(handler, &request, &response, &error));
    assert(response.status_code == 405);
    assert(strcmp(response.content_type, "application/json; charset=utf-8") == 0);
    assert(strcmp(response.body,
                  "{\"ok\":false,\"error\":{\"message\":\"Only POST requests are supported.\"}}")
           == 0);

    http_response_destroy(&response);
    http_request_destroy(&request);
    destroy_handler(handler, service, guard);
}

static void test_request_handler_rejects_blank_body(void) {
    SqlService *service = NULL;
    DbGuard *guard = NULL;
    RequestHandler *handler = create_handler(&service, &guard);
    SqlError error = {0};
    HttpRequest request = {0};
    HttpResponse response = {0};

    strncpy(request.method, "POST", sizeof(request.method) - 1);
    strncpy(request.path, "/query", sizeof(request.path) - 1);
    request.body = sql_strdup("   \n\t");
    request.body_length = strlen(request.body);

    assert(request_handler_handle(handler, &request, &response, &error));
    assert(response.status_code == 400);
    assert(strcmp(response.body,
                  "{\"ok\":false,\"error\":{\"message\":\"Request body must contain SQL text.\"}}")
           == 0);

    http_response_destroy(&response);
    http_request_destroy(&request);
    destroy_handler(handler, service, guard);
}

static void test_request_handler_rejects_invalid_path(void) {
    SqlService *service = NULL;
    DbGuard *guard = NULL;
    RequestHandler *handler = create_handler(&service, &guard);
    SqlError error = {0};
    HttpRequest request = {0};
    HttpResponse response = {0};

    strncpy(request.method, "POST", sizeof(request.method) - 1);
    strncpy(request.path, "query", sizeof(request.path) - 1);
    request.body = sql_strdup("SELECT * FROM users;");
    request.body_length = strlen(request.body);

    assert(request_handler_handle(handler, &request, &response, &error));
    assert(response.status_code == 400);
    assert(strcmp(response.body,
                  "{\"ok\":false,\"error\":{\"message\":\"Request path must start with '/'.\"}}")
           == 0);

    http_response_destroy(&response);
    http_request_destroy(&request);
    destroy_handler(handler, service, guard);
}

static void test_request_handler_rejects_unsupported_sql(void) {
    SqlService *service = NULL;
    DbGuard *guard = NULL;
    RequestHandler *handler = create_handler(&service, &guard);
    SqlError error = {0};
    HttpRequest request = {0};
    HttpResponse response = {0};

    strncpy(request.method, "POST", sizeof(request.method) - 1);
    strncpy(request.path, "/query", sizeof(request.path) - 1);
    request.body = sql_strdup("DROP TABLE users;");
    request.body_length = strlen(request.body);

    assert(request_handler_handle(handler, &request, &response, &error));
    assert(response.status_code == 400);
    assert(strcmp(response.body,
                  "{\"ok\":false,\"error\":{\"message\":\"sql_service_classify_operation could not classify SQL\"}}")
           == 0);

    http_response_destroy(&response);
    http_request_destroy(&request);
    destroy_handler(handler, service, guard);
}

int main(void) {
    test_request_handler_returns_200_for_valid_select();
    test_request_handler_rejects_non_post_method();
    test_request_handler_rejects_blank_body();
    test_request_handler_rejects_invalid_path();
    test_request_handler_rejects_unsupported_sql();
    printf("request handler unit tests passed\n");
    return 0;
}
