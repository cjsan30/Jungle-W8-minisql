#include "sql_service.h"
#include <ctype.h>

static const char *skip_sql_prefix(const char *sql_text) {
    const char *cursor = sql_text;

    for (;;) {
        while (*cursor != '\0' && isspace((unsigned char) *cursor)) {
            cursor++;
        }

        if (cursor[0] == '-' && cursor[1] == '-') {
            cursor += 2;
            while (*cursor != '\0' && *cursor != '\n') {
                cursor++;
            }
            continue;
        }

        if (*cursor == ';') {
            cursor++;
            continue;
        }

        return cursor;
    }
}

static int is_sql_identifier_char(unsigned char ch) {
    return isalnum(ch) || ch == '_';
}

static int matches_keyword(const char *text, const char *keyword) {
    size_t index;

    for (index = 0; keyword[index] != '\0'; index++) {
        if (toupper((unsigned char) text[index]) != keyword[index]) {
            return 0;
        }
    }

    return !is_sql_identifier_char((unsigned char) text[index]);
}

/*
 * 기능:
 * - 기존 DB 엔진을 재사용하는 SQL 서비스 객체를 생성한다.
 *
 * 반환값:
 * - 성공: `SqlService *`
 * - 실패: `NULL`, error에 원인 기록
 *
 * 흐름:
 * - DB root를 검증한다.
 * - `DbContext`와 서비스 상태를 준비한다.
 *
 * 현재 상태:
 * - DB root와 서비스 상태만 안전하게 준비한다.
 * - 실제 DB 컨텍스트 연결은 후속 단계에서 붙인다.
 */
SqlService *sql_service_create(const char *db_root, SqlError *error) {
    SqlService *service;
    size_t root_length;

    if (db_root == NULL || db_root[0] == '\0') {
        sql_set_error(error, 0, 0, "sql_service_create requires a db_root");
        return NULL;
    }

    root_length = strlen(db_root);
    if (root_length >= sizeof(service->db_root)) {
        sql_set_error(error, 0, 0, "db_root path is too long");
        return NULL;
    }

    service = (SqlService *) malloc(sizeof(*service));
    if (service == NULL) {
        sql_set_error(error, 0, 0, "failed to allocate SQL service");
        return NULL;
    }

    memset(service, 0, sizeof(*service));
    memcpy(service->db_root, db_root, root_length + 1U);
    return service;
}

/*
 * 기능:
 * - SQL 문자열을 읽기/쓰기 동작으로 분류한다.
 *
 * 반환값:
 * - 성공: 1
 * - 실패: 0, error에 원인 기록
 *
 * 흐름:
 * - SQL 시작 키워드를 확인한다.
 * - SELECT면 READ, INSERT면 WRITE로 분류한다.
 *
 * 현재 상태:
 * - 협업용 공용 stub 함수다.
 */
int sql_service_classify_operation(const char *sql_text, SqlOperationKind *operation, SqlError *error) {
    const char *cursor;

    if (sql_text == NULL || operation == NULL) {
        sql_set_error(error, 0, 0, "sql_service_classify_operation received invalid arguments");
        return 0;
    }

    cursor = skip_sql_prefix(sql_text);
    if (*cursor == '\0') {
        *operation = SQL_OPERATION_UNKNOWN;
        sql_set_error(error, 0, 0, "sql_service_classify_operation received empty SQL");
        return 0;
    }

    if (matches_keyword(cursor, "SELECT")) {
        *operation = SQL_OPERATION_READ;
        return 1;
    }

    if (matches_keyword(cursor, "INSERT")) {
        *operation = SQL_OPERATION_WRITE;
        return 1;
    }

    *operation = SQL_OPERATION_UNKNOWN;
    sql_set_error(error, 0, 0, "sql_service_classify_operation could not classify SQL");
    return 0;
}

/*
 * 기능:
 * - SQL 문자열 1건을 실행하고 문자열 결과를 돌려준다.
 *
 * 반환값:
 * - 성공: `SqlServiceResult`
 * - 실패: `ok=0`, error에 원인 기록
 *
 * 흐름:
 * - SQL 문자열을 파싱한다.
 * - DB 엔진을 실행한다.
 * - 결과를 응답용 문자열로 직렬화한다.
 *
 * 현재 상태:
 * - 분류/검증 경계만 유지하고 실제 DB 실행 연결은 보류한다.
 */
SqlServiceResult sql_service_execute(SqlService *service, const char *sql_text, SqlError *error) {
    SqlServiceResult result = {0};
    SqlOperationKind operation = SQL_OPERATION_UNKNOWN;

    if (service == NULL || sql_text == NULL) {
        sql_set_error(error, 0, 0, "sql_service_execute received invalid arguments");
        result.payload = sql_strdup("sql_service_execute received invalid arguments");
        return result;
    }

    if (!sql_service_classify_operation(sql_text, &operation, error)) {
        result.payload = error != NULL ? sql_strdup(error->message) : NULL;
        return result;
    }

    (void) operation;
    sql_set_error(error, 0, 0, "sql_service_execute is not integrated with DB execution yet");
    result.payload = sql_strdup("DB execution bridge is not ready.");
    return result;
}

/*
 * 기능:
 * - SQL 실행 결과 내부 메모리를 해제한다.
 *
 * 반환값:
 * - 없음
 *
 * 흐름:
 * - payload를 해제한다.
 * - 상태값을 초기화한다.
 */
void sql_service_result_destroy(SqlServiceResult *result) {
    if (result == NULL) {
        return;
    }

    free(result->payload);
    result->payload = NULL;
    result->ok = 0;
}

/*
 * 기능:
 * - SQL 서비스 객체와 내부 상태를 해제한다.
 *
 * 반환값:
 * - 없음
 *
 * 흐름:
 * - `DbContext` 등 내부 자원을 정리한다.
 * - 마지막에 서비스 구조체를 해제한다.
 *
 * 현재 상태:
 * - 서비스 내부 포인터가 연결된 경우까지 안전하게 정리한다.
 */
void sql_service_destroy(SqlService *service) {
    if (service == NULL) {
        return;
    }

    if (service->db_context != NULL) {
        db_context_destroy(service->db_context);
        service->db_context = NULL;
    }

    free(service);
}
