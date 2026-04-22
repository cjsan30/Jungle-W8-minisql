#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "sql_service.h"

static void test_sql_service_executes_select_query(void) {
    SqlError error = {0};
    SqlService *service = sql_service_create("./data", &error);
    SqlServiceResult result;

    assert(service != NULL);

    result = sql_service_execute(service, "SELECT * FROM users WHERE id = 2;", &error);
    assert(result.ok == 1);
    assert(result.payload != NULL);
    assert(strcmp(result.payload,
                  "{\"ok\":true,\"type\":\"select\",\"table\":\"users\",\"rows\":[{\"id\":2,\"name\":\"Bob\",\"age\":22}]}")
           == 0);

    sql_service_result_destroy(&result);
    sql_service_destroy(service);
}

static void test_sql_service_reports_invalid_sql(void) {
    SqlError error = {0};
    SqlService *service = sql_service_create("./data", &error);
    SqlServiceResult result;

    assert(service != NULL);

    result = sql_service_execute(service, "DROP TABLE users;", &error);
    assert(result.ok == 0);
    assert(result.payload != NULL);
    assert(strcmp(result.payload, "{\"ok\":false,\"error\":{\"message\":\"unsupported SQL statement\"}}") == 0);

    sql_service_result_destroy(&result);
    sql_service_destroy(service);
}

static void test_sql_service_reports_invalid_where_id_type(void) {
    SqlError error = {0};
    SqlService *service = sql_service_create("./data", &error);
    SqlServiceResult result;

    assert(service != NULL);

    result = sql_service_execute(service, "SELECT * FROM users WHERE id = 'abc';", &error);
    assert(result.ok == 0);
    assert(result.payload != NULL);
    assert(strcmp(result.payload, "{\"ok\":false,\"error\":{\"message\":\"WHERE id requires an integer value\"}}")
           == 0);

    sql_service_result_destroy(&result);
    sql_service_destroy(service);
}

static void test_sql_service_reports_unknown_where_column(void) {
    SqlError error = {0};
    SqlService *service = sql_service_create("./data", &error);
    SqlServiceResult result;

    assert(service != NULL);

    result = sql_service_execute(service, "SELECT * FROM users WHERE nickname = 'Bob';", &error);
    assert(result.ok == 0);
    assert(result.payload != NULL);
    assert(strcmp(result.payload,
                  "{\"ok\":false,\"error\":{\"message\":\"unknown column in WHERE clause: nickname\"}}")
           == 0);

    sql_service_result_destroy(&result);
    sql_service_destroy(service);
}

static void test_sql_json_quote_escapes_special_characters(void) {
    SqlError error = {0};
    char *quoted = sql_json_quote("A\"B\\C\n", &error);

    assert(quoted != NULL);
    assert(strcmp(quoted, "\"A\\\"B\\\\C\\n\"") == 0);

    free(quoted);
}

int main(void) {
    test_sql_service_executes_select_query();
    test_sql_service_reports_invalid_sql();
    test_sql_service_reports_invalid_where_id_type();
    test_sql_service_reports_unknown_where_column();
    test_sql_json_quote_escapes_special_characters();
    printf("sql service unit tests passed\n");
    return 0;
}
