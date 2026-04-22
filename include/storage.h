#ifndef STORAGE_H
#define STORAGE_H

#include "common.h"

typedef struct {
    char **fields;
    int field_count;
} Row;

typedef struct {
    Row *rows;
    int row_count;
    int row_capacity;
    int column_count;
} RowSet;

/*
 * 동시성 보호 계약(4번 개발자 기준):
 * - storage 계층은 파일 I/O 보호 lock을 직접 관리하지 않는다.
 * - append_csv_row 호출은 반드시 write lock 구간에서만 수행한다.
 * - read_csv_rows 호출은 초기 로드/리로드 시점에 read/write 정책에 맞춰
 *   상위(db_context + db_guard)에서 직렬화해 호출한다.
 */
int append_csv_row(const char *db_root, const char *table_name, char **fields, int field_count, SqlError *error);
int read_csv_rows(const char *db_root, const char *table_name, int expected_fields, RowSet *rowset, SqlError *error);
void free_rowset(RowSet *rowset);

#endif
