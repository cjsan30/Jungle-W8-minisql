#include "sql_service.h"

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
 * - SQL 서비스 생성을 위한 stub 함수다.
 */
SqlService *sql_service_create(const char *db_root, SqlError *error) {
    (void) db_root;
    sql_set_error(error, 0, 0, "sql_service_create stub: SQL 서비스 초기화 로직이 아직 구현되지 않았습니다");
    return NULL;
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
    if (sql_text == NULL || operation == NULL) {
        sql_set_error(error, 0, 0, "sql_service_classify_operation received invalid arguments");
        return 0;
    }

    while (*sql_text == ' ' || *sql_text == '\n' || *sql_text == '\t' || *sql_text == '\r') {
        sql_text++;
    }

    if (strncmp(sql_text, "SELECT", 6) == 0) {
        *operation = SQL_OPERATION_READ;
        return 1;
    }

    if (strncmp(sql_text, "INSERT", 6) == 0) {
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
 * - SQL 실행 진입점을 위한 stub 함수다.
 */
SqlServiceResult sql_service_execute(SqlService *service, const char *sql_text, SqlError *error) {
    SqlServiceResult result = {0};

    (void) service;
    (void) sql_text;
    sql_set_error(error, 0, 0, "sql_service_execute stub: SQL 파싱/실행/결과 직렬화 로직이 아직 구현되지 않았습니다");
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
 * - 해제 지점을 위한 stub 함수다.
 */
void sql_service_destroy(SqlService *service) {
    (void) service;
}
