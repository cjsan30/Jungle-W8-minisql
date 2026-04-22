#ifndef DB_GUARD_H
#define DB_GUARD_H

#include "common.h"
#include "db_context.h"

typedef struct DbGuard DbGuard;
typedef int (*DbGuardWorkFn)(void *work_ctx, SqlError *error);

/*
 * 기능:
 * - DB 공유 자원 보호 계층을 생성한다.
 * - SELECT 병렬 처리와 INSERT 직렬화 같은 동시성 정책의 진입점이다.
 *
 * 반환값:
 * - 성공: DbGuard 포인터
 * - 실패: NULL, error에 실패 원인 기록
 */
DbGuard *db_guard_create(DbContext *ctx, SqlError *error);

/*
 * 기능:
 * - 읽기 작업 시작 전에 read lock을 획득한다.
 *
 * 반환값:
 * - 성공: 1
 * - 실패: 0, error에 실패 원인 기록
 */
int db_guard_lock_read(DbGuard *guard, SqlError *error);

/*
 * 기능:
 * - 쓰기 작업 시작 전에 write lock을 획득한다.
 *
 * 반환값:
 * - 성공: 1
 * - 실패: 0, error에 실패 원인 기록
 */
int db_guard_lock_write(DbGuard *guard, SqlError *error);

/*
 * 기능:
 * - 읽기 락을 해제한다.
 *
 * 반환값:
 * - 없음
 */
void db_guard_unlock_read(DbGuard *guard);

/*
 * 기능:
 * - 쓰기 락을 해제한다.
 *
 * 반환값:
 * - 없음
 */
void db_guard_unlock_write(DbGuard *guard);

/*
 * 기능:
 * - 보호 계층을 해제한다.
 *
 * 반환값:
 * - 없음
 */
void db_guard_destroy(DbGuard *guard);

/*
 * 기능:
 * - read lock을 잡은 상태에서 작업 함수를 실행한다.
 * - 4번이 3번에게 전달하는 공용 호출 경계 stub다.
 *
 * 반환값:
 * - 성공: work_fn의 반환값
 * - 실패: 0, error에 원인 기록
 *
 * 흐름:
 * - read lock을 획득한다.
 * - 작업 함수를 실행한다.
 * - read lock을 해제한다.
 */
int db_guard_execute_read(DbGuard *guard, DbGuardWorkFn work_fn, void *work_ctx, SqlError *error);

/*
 * 기능:
 * - write lock을 잡은 상태에서 작업 함수를 실행한다.
 * - 4번이 3번에게 전달하는 공용 호출 경계 stub다.
 *
 * 반환값:
 * - 성공: work_fn의 반환값
 * - 실패: 0, error에 원인 기록
 *
 * 흐름:
 * - write lock을 획득한다.
 * - 작업 함수를 실행한다.
 * - write lock을 해제한다.
 */
int db_guard_execute_write(DbGuard *guard, DbGuardWorkFn work_fn, void *work_ctx, SqlError *error);

#endif
