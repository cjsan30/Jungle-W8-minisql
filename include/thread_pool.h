#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include "common.h"
#include "job_queue.h"

typedef struct ThreadPool ThreadPool;

/*
 * 기능:
 * - 고정 개수 worker를 가지는 스레드풀을 생성한다.
 *
 * 반환값:
 * - 성공: ThreadPool 포인터
 * - 실패: NULL, error에 실패 원인 기록
 */
ThreadPool *thread_pool_create(int worker_count, int queue_capacity, SqlError *error);

/*
 * 기능:
 * - 스레드풀을 시작한다.
 *
 * 반환값:
 * - 성공: 1
 * - 실패: 0, error에 실패 원인 기록
 */
int thread_pool_start(ThreadPool *pool, SqlError *error);

/*
 * 기능:
 * - 작업을 worker queue에 제출한다.
 *
 * 반환값:
 * - 성공: 1
 * - 실패: 0, error에 실패 원인 기록
 */
int thread_pool_submit(ThreadPool *pool, Job job, SqlError *error);

/*
 * 기능:
 * - 스레드풀을 종료한다.
 *
 * 반환값:
 * - 없음
 */
void thread_pool_stop(ThreadPool *pool);

/*
 * 기능:
 * - 스레드풀 메모리를 해제한다.
 *
 * 반환값:
 * - 없음
 */
void thread_pool_destroy(ThreadPool *pool);

#endif
