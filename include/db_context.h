#ifndef DB_CONTEXT_H
#define DB_CONTEXT_H

#include "bptree.h"
#include "schema.h"
#include "storage.h"

typedef struct {
    char name[256];
    Schema schema;
    RowSet rowset;
    BPTree *index;
    int next_id;
} TableState;

typedef struct {
    char db_root[SQL_PATH_BUFFER_SIZE];
    TableState *tables;
    int table_count;
} DbContext;

/*
 * 동시성 보호 계약(4번 개발자 기준):
 * - DbContext는 자체적으로 lock을 제공하지 않는다.
 * - 서버 경로에서 아래 함수 호출은 반드시 db_guard 보호 구간 안에서 수행한다.
 *   - db_context_get_table
 *   - db_context_reload_table
 * - 권장 경계:
 *   - SELECT 경로: db_guard_execute_read 내부에서 호출
 *   - INSERT 경로: db_guard_execute_write 내부에서 호출
 */
DbContext *db_context_create(const char *db_root, SqlError *error);
void db_context_destroy(DbContext *ctx);
TableState *db_context_get_table(DbContext *ctx, const char *table_name, SqlError *error);
int db_context_reload_table(DbContext *ctx, const char *table_name, SqlError *error);

#endif
