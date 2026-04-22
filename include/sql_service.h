#ifndef SQL_SERVICE_H
#define SQL_SERVICE_H

#include "common.h"
#include "db_context.h"

typedef struct {
    int ok;
    char *payload;
} SqlServiceResult;

typedef enum {
    SQL_OPERATION_UNKNOWN = 0,
    SQL_OPERATION_READ = 1,
    SQL_OPERATION_WRITE = 2
} SqlOperationKind;

typedef struct {
    DbContext *db_context;
    char db_root[SQL_PATH_BUFFER_SIZE];
} SqlService;

/*
 * 기능:
 * - 기존 CLI 중심 SQL 실행기를 서버에서도 재사용할 수 있는 서비스 객체를 만든다.
 *
 * 반환값:
 * - 성공: SqlService 포인터
 * - 실패: NULL, error에 실패 원인 기록
 */
SqlService *sql_service_create(const char *db_root, SqlError *error);

/*
 * 기능:
 * - SQL 문자열이 읽기/쓰기 중 어느 동작인지 분류한다.
 * - 3번과 4번이 공유하는 락 분기용 공용 stub다.
 *
 * 반환값:
 * - 성공: 1
 * - 실패: 0, error에 원인 기록
 *
 * 흐름:
 * - SQL 앞부분 키워드를 읽는다.
 * - SELECT면 READ, INSERT면 WRITE로 분류한다.
 */
int sql_service_classify_operation(const char *sql_text, SqlOperationKind *operation, SqlError *error);

/*
 * 기능:
 * - SQL 문자열 1건을 실행하고 문자열 결과를 돌려준다.
 * - 외부 API 요청에서 가장 직접적으로 호출될 함수다.
 *
 * 반환값:
 * - 성공: ok=1, payload에 결과 문자열
 * - 실패: ok=0, payload에 에러 문자열 또는 NULL
 *
 * 흐름:
 * - SQL 문자열을 파싱한다.
 * - executor와 db_context를 이용해 실제 DB 엔진을 실행한다.
 * - 결과를 응답용 문자열로 직렬화한다.
 */
SqlServiceResult sql_service_execute(SqlService *service, const char *sql_text, SqlError *error);

/*
 * 기능:
 * - SQL 실행 결과 구조체 내부 메모리를 해제한다.
 *
 * 반환값:
 * - 없음
 */
void sql_service_result_destroy(SqlServiceResult *result);

/*
 * 기능:
 * - SqlService와 내부 DbContext를 정리한다.
 *
 * 반환값:
 * - 없음
 */
void sql_service_destroy(SqlService *service);

#endif
