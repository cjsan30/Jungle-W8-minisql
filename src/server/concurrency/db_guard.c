#include "db_guard.h"
#include <stdatomic.h>

struct DbGuard {
    DbContext *ctx;
    atomic_int reader_count;
    atomic_int writer_active;
    atomic_int writers_waiting;
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
 * - 서버 요청 경로에서 사용할 동시성 보호 계층을 초기화한다.
 */
DbGuard *db_guard_create(DbContext *ctx, SqlError *error) {
    DbGuard *guard;

    if (ctx == NULL) {
        sql_set_error(error, 0, 0, "db_guard_create received NULL db context");
        return NULL;
    }

    guard = (DbGuard *) calloc(1, sizeof(DbGuard));
    if (guard == NULL) {
        sql_set_error(error, 0, 0, "db_guard_create failed to allocate guard");
        return NULL;
    }

    guard->ctx = ctx;
    atomic_init(&guard->reader_count, 0);
    atomic_init(&guard->writer_active, 0);
    atomic_init(&guard->writers_waiting, 0);
    return guard;
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
 * - writer 우선 정책을 반영해 read lock을 획득한다.
 */
int db_guard_lock_read(DbGuard *guard, SqlError *error) {
    if (guard == NULL) {
        sql_set_error(error, 0, 0, "db_guard_lock_read received NULL guard");
        return 0;
    }

    for (;;) {
        if (atomic_load_explicit(&guard->writer_active, memory_order_acquire) != 0 ||
            atomic_load_explicit(&guard->writers_waiting, memory_order_acquire) != 0) {
            continue;
        }

        (void) atomic_fetch_add_explicit(&guard->reader_count, 1, memory_order_acquire);
        if (atomic_load_explicit(&guard->writer_active, memory_order_acquire) == 0 &&
            atomic_load_explicit(&guard->writers_waiting, memory_order_acquire) == 0) {
            return 1;
        }

        (void) atomic_fetch_sub_explicit(&guard->reader_count, 1, memory_order_release);
    }
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
 * - reader가 비워질 때까지 기다린 뒤 write lock을 획득한다.
 */
int db_guard_lock_write(DbGuard *guard, SqlError *error) {
    if (guard == NULL) {
        sql_set_error(error, 0, 0, "db_guard_lock_write received NULL guard");
        return 0;
    }

    (void) atomic_fetch_add_explicit(&guard->writers_waiting, 1, memory_order_acq_rel);

    for (;;) {
        int expected = 0;

        if (!atomic_compare_exchange_weak_explicit(
                &guard->writer_active,
                &expected,
                1,
                memory_order_acq_rel,
                memory_order_acquire)) {
            continue;
        }

        if (atomic_load_explicit(&guard->reader_count, memory_order_acquire) == 0) {
            (void) atomic_fetch_sub_explicit(&guard->writers_waiting, 1, memory_order_acq_rel);
            return 1;
        }

        atomic_store_explicit(&guard->writer_active, 0, memory_order_release);
    }
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
 * - 다음 reader 또는 writer가 진입할 수 있도록 상태를 반영한다.
 *
 * 현재 상태:
 * - read lock 해제만 담당한다.
 */
void db_guard_unlock_read(DbGuard *guard) {
    if (guard == NULL) {
        return;
    }

    (void) atomic_fetch_sub_explicit(&guard->reader_count, 1, memory_order_release);
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
 * - 다음 reader 또는 writer가 진입할 수 있도록 상태를 반영한다.
 *
 * 현재 상태:
 * - write lock 해제만 담당한다.
 */
void db_guard_unlock_write(DbGuard *guard) {
    if (guard == NULL) {
        return;
    }

    atomic_store_explicit(&guard->writer_active, 0, memory_order_release);
}

/*
 * 기능:
 * - 동시성 보호 계층 객체를 해제한다.
 *
 * 반환값:
 * - 없음
 *
 * 흐름:
 * - 내부 상태를 사용 종료 상태로 정리한다.
 * - 마지막에 guard 구조체를 해제한다.
 *
 * 현재 상태:
 * - guard 구조체 메모리를 해제한다.
 */
void db_guard_destroy(DbGuard *guard) {
    if (guard == NULL) {
        return;
    }

    free(guard);
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
 * - request handler의 읽기 SQL 실행 경계로 사용된다.
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
 * - request handler의 쓰기 SQL 실행 경계로 사용된다.
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
