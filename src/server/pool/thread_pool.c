#include "thread_pool.h"

struct ThreadPool {
    int worker_count;
};

/*
 * 기능:
 * - 고정 크기 worker를 가지는 thread pool 객체를 생성한다.
 *
 * 반환값:
 * - 성공: `ThreadPool *`
 * - 실패: `NULL`, error에 원인 기록
 *
 * 흐름:
 * - worker 수와 queue 크기를 검증한다.
 * - pool 구조체와 queue를 준비한다.
 * - 이후 `thread_pool_start`에서 worker를 실제 시작한다.
 *
 * 현재 상태:
 * - thread pool 생성을 위한 stub 함수다.
 */
ThreadPool *thread_pool_create(int worker_count, int queue_capacity, SqlError *error) {
    (void) worker_count;
    (void) queue_capacity;
    sql_set_error(error, 0, 0, "thread_pool_create stub: 스레드풀 생성 로직이 아직 구현되지 않았습니다");
    return NULL;
}

/*
 * 기능:
 * - worker thread를 시작한다.
 *
 * 반환값:
 * - 성공: 1
 * - 실패: 0, error에 원인 기록
 *
 * 흐름:
 * - worker 수만큼 스레드를 만든다.
 * - 각 worker가 queue를 소비하도록 연결한다.
 *
 * 현재 상태:
 * - worker 시작을 위한 stub 함수다.
 */
int thread_pool_start(ThreadPool *pool, SqlError *error) {
    (void) pool;
    sql_set_error(error, 0, 0, "thread_pool_start stub: worker thread 시작 로직이 아직 구현되지 않았습니다");
    return 0;
}

/*
 * 기능:
 * - 작업 1건을 thread pool queue에 제출한다.
 *
 * 반환값:
 * - 성공: 1
 * - 실패: 0, error에 원인 기록
 *
 * 흐름:
 * - 입력 job의 유효성을 검사한다.
 * - queue에 작업을 넣는다.
 * - 대기 중인 worker가 있으면 깨운다.
 *
 * 현재 상태:
 * - 작업 제출을 위한 stub 함수다.
 */
int thread_pool_submit(ThreadPool *pool, Job job, SqlError *error) {
    (void) pool;
    (void) job;
    sql_set_error(error, 0, 0, "thread_pool_submit stub: 작업 제출 로직이 아직 구현되지 않았습니다");
    return 0;
}

/*
 * 기능:
 * - thread pool을 중지한다.
 *
 * 반환값:
 * - 없음
 *
 * 흐름:
 * - 새 작업 수신을 막는다.
 * - worker 종료를 유도한다.
 *
 * 현재 상태:
 * - 종료 지점을 위한 stub 함수다.
 */
void thread_pool_stop(ThreadPool *pool) {
    (void) pool;
}

/*
 * 기능:
 * - thread pool 메모리를 해제한다.
 *
 * 반환값:
 * - 없음
 *
 * 흐름:
 * - 내부 queue와 worker 관련 자원을 정리한다.
 * - 마지막에 pool 구조체를 해제한다.
 *
 * 현재 상태:
 * - 해제 지점을 위한 stub 함수다.
 */
void thread_pool_destroy(ThreadPool *pool) {
    (void) pool;
}
