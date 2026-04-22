#include "db_guard.h"

struct DbGuard {
    DbContext *ctx;
};

/*
 * 기능:
 * - DB 동시성 보호 계층 객체를 생성한다.
 *
 * 반환값:
 * - 성공: `DbGuard *`
 * - 실패: `NULL`, error에 원인 기록
 *
 * 흐름:
 * - 보호 대상 `DbContext`를 연결한다.
 * - read/write lock 상태를 초기화한다.
 *
 * 현재 상태:
 * - 동시성 보호 계층 생성을 위한 stub 함수다.
 */
DbGuard *db_guard_create(DbContext *ctx, SqlError *error) {
    (void) ctx;
    sql_set_error(error, 0, 0, "db_guard_create stub: DB 동시성 보호 계층이 아직 구현되지 않았습니다");
    return NULL;
}

/*
 * 기능:
 * - 읽기 작업 전에 read lock을 획득한다.
 *
 * 반환값:
 * - 성공: 1
 * - 실패: 0, error에 원인 기록
 *
 * 흐름:
 * - 현재 쓰기 작업 여부를 확인한다.
 * - 읽기 가능 상태면 lock을 획득한다.
 *
 * 현재 상태:
 * - read lock 획득을 위한 stub 함수다.
 */
int db_guard_lock_read(DbGuard *guard, SqlError *error) {
    (void) guard;
    sql_set_error(error, 0, 0, "db_guard_lock_read stub: read lock 로직이 아직 구현되지 않았습니다");
    return 0;
}

/*
 * 기능:
 * - 쓰기 작업 전에 write lock을 획득한다.
 *
 * 반환값:
 * - 성공: 1
 * - 실패: 0, error에 원인 기록
 *
 * 흐름:
 * - 현재 읽기/쓰기 상태를 확인한다.
 * - 단독 접근이 가능할 때 lock을 획득한다.
 *
 * 현재 상태:
 * - write lock 획득을 위한 stub 함수다.
 */
int db_guard_lock_write(DbGuard *guard, SqlError *error) {
    (void) guard;
    sql_set_error(error, 0, 0, "db_guard_lock_write stub: write lock 로직이 아직 구현되지 않았습니다");
    return 0;
}

/*
 * 기능:
 * - read lock을 해제한다.
 *
 * 반환값:
 * - 없음
 *
 * 흐름:
 * - 읽기 카운트나 상태를 줄인다.
 * - 대기 중인 writer가 있으면 깨운다.
 *
 * 현재 상태:
 * - 해제 지점을 위한 stub 함수다.
 */
void db_guard_unlock_read(DbGuard *guard) {
    (void) guard;
}

/*
 * 기능:
 * - write lock을 해제한다.
 *
 * 반환값:
 * - 없음
 *
 * 흐름:
 * - 쓰기 상태를 종료한다.
 * - 대기 중인 reader/writer를 깨운다.
 *
 * 현재 상태:
 * - 해제 지점을 위한 stub 함수다.
 */
void db_guard_unlock_write(DbGuard *guard) {
    (void) guard;
}

/*
 * 기능:
 * - 동시성 보호 계층 객체를 해제한다.
 *
 * 반환값:
 * - 없음
 *
 * 흐름:
 * - 내부 lock 자원을 정리한다.
 * - 마지막에 guard 구조체를 해제한다.
 *
 * 현재 상태:
 * - 해제 지점을 위한 stub 함수다.
 */
void db_guard_destroy(DbGuard *guard) {
    (void) guard;
}

/*
 * 기능:
 * - read lock으로 감싼 작업 실행 흐름을 제공한다.
 *
 * 반환값:
 * - 성공: work_fn의 반환값
 * - 실패: 0, error에 원인 기록
 *
 * 흐름:
 * - read lock을 획득한다.
 * - 작업 함수를 호출한다.
 * - read lock을 해제한다.
 *
 * 현재 상태:
 * - 협업용 공용 stub 함수다.
 */
int db_guard_execute_read(DbGuard *guard, DbGuardWorkFn work_fn, void *work_ctx, SqlError *error) {
    int ok;

    if (guard == NULL || work_fn == NULL) {
        sql_set_error(error, 0, 0, "db_guard_execute_read received invalid arguments");
        return 0;
    }

    if (!db_guard_lock_read(guard, error)) {
        return 0;
    }

    ok = work_fn(work_ctx, error);
    db_guard_unlock_read(guard);
    return ok;
}

/*
 * 기능:
 * - write lock으로 감싼 작업 실행 흐름을 제공한다.
 *
 * 반환값:
 * - 성공: work_fn의 반환값
 * - 실패: 0, error에 원인 기록
 *
 * 흐름:
 * - write lock을 획득한다.
 * - 작업 함수를 호출한다.
 * - write lock을 해제한다.
 *
 * 현재 상태:
 * - 협업용 공용 stub 함수다.
 */
int db_guard_execute_write(DbGuard *guard, DbGuardWorkFn work_fn, void *work_ctx, SqlError *error) {
    int ok;

    if (guard == NULL || work_fn == NULL) {
        sql_set_error(error, 0, 0, "db_guard_execute_write received invalid arguments");
        return 0;
    }

    if (!db_guard_lock_write(guard, error)) {
        return 0;
    }

    ok = work_fn(work_ctx, error);
    db_guard_unlock_write(guard);
    return ok;
}
