# 4번 DB 동시성 경계 확정서

이 문서는 `src/server/concurrency/.ready` 생성 기준 중
`공용 접점 stub + 호출 순서 + 보호 지점`을 명시한다.

## 1) 공개 인터페이스

- `include/db_guard.h`
  - `db_guard_create`
  - `db_guard_lock_read`
  - `db_guard_lock_write`
  - `db_guard_unlock_read`
  - `db_guard_unlock_write`
  - `db_guard_destroy`
  - `db_guard_execute_read`
  - `db_guard_execute_write`

## 2) SQL 연산별 lock 정책

- `SELECT` -> `read lock`
- `INSERT` -> `write lock`
- lock 획득/해제 책임은 `db_guard_execute_*` 내부에 둔다.

## 3) 3번-4번 호출 순서(요청 계층 연결 기준)

1. `sql_service_classify_operation(sql, &kind, error)`
2. `kind == SQL_OPERATION_READ`
   - `db_guard_execute_read(guard, work_fn, work_ctx, error)`
3. `kind == SQL_OPERATION_WRITE`
   - `db_guard_execute_write(guard, work_fn, work_ctx, error)`

`work_fn` 내부에서 기존 DB 엔진(`executor/db_context/storage`) 경로를 호출한다.

## 4) 공용 DB 보호 지점

- `src/query/executor.c`
  - `execute_query` 전체 호출을 lock 경계로 보호
- `include/db_context.h`
  - `db_context_get_table`, `db_context_reload_table`는 상위 lock 보호 하에서 호출
- `include/storage.h`
  - `append_csv_row`는 write lock 구간에서 호출

## 5) lock/unlock 기본 경로 확인

- 구현 위치: `src/server/concurrency/db_guard.c`
- 확인 포인트:
  - read 경로: `db_guard_lock_read` -> `db_guard_unlock_read`
  - write 경로: `db_guard_lock_write` -> `db_guard_unlock_write`
  - 공용 경로: `db_guard_execute_read/write`가 lock/unlock을 항상 감싼다.
