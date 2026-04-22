#include "thread_pool.h"

#include <pthread.h>

struct ThreadPool {
    int worker_count;
    int started;
    JobQueue *queue;
    pthread_t *workers;
};

static void run_job(Job *job) {
    if (job == NULL) {
        return;
    }

    job->execute(job->data);
    if (job->cleanup != NULL) {
        job->cleanup(job->data);
    }
}

static void *thread_pool_worker_main(void *arg) {
    ThreadPool *pool = arg;
    Job job = {0};

    while (job_queue_pop(pool->queue, &job)) {
        run_job(&job);
        memset(&job, 0, sizeof(job));
    }

    return NULL;
}

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
 * 구현 상태:
 * - thread pool과 내부 queue를 생성한다.
 */
ThreadPool *thread_pool_create(int worker_count, int queue_capacity, SqlError *error) {
    ThreadPool *pool = NULL;

    if (worker_count <= 0) {
        sql_set_error(error, 0, 0, "worker count must be positive");
        return NULL;
    }
    if (queue_capacity <= 0) {
        sql_set_error(error, 0, 0, "queue capacity must be positive");
        return NULL;
    }

    pool = calloc(1U, sizeof(*pool));
    if (pool == NULL) {
        sql_set_error(error, 0, 0, "failed to allocate thread pool");
        return NULL;
    }

    pool->workers = calloc((size_t) worker_count, sizeof(*pool->workers));
    if (pool->workers == NULL) {
        sql_set_error(error, 0, 0, "failed to allocate thread pool workers");
        free(pool);
        return NULL;
    }

    pool->queue = job_queue_create(queue_capacity, error);
    if (pool->queue == NULL) {
        free(pool->workers);
        free(pool);
        return NULL;
    }

    pool->worker_count = worker_count;
    return pool;
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
 * 구현 상태:
 * - worker thread를 시작하고 queue pop 루프에 연결한다.
 */
int thread_pool_start(ThreadPool *pool, SqlError *error) {
    int index = 0;

    if (pool == NULL) {
        sql_set_error(error, 0, 0, "thread pool must not be null");
        return 0;
    }
    if (pool->started) {
        return 1;
    }

    pool->started = 1;
    for (index = 0; index < pool->worker_count; index++) {
        if (pthread_create(&pool->workers[index], NULL, thread_pool_worker_main, pool) != 0) {
            sql_set_error(error, 0, 0, "failed to start worker thread");
            pool->worker_count = index;
            thread_pool_stop(pool);
            return 0;
        }
    }

    return 1;
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
 * 구현 상태:
 * - 입력 job을 검증하고 내부 queue에 제출한다.
 */
int thread_pool_submit(ThreadPool *pool, Job job, SqlError *error) {
    if (pool == NULL) {
        sql_set_error(error, 0, 0, "thread pool must not be null");
        if (job.cleanup != NULL) {
            job.cleanup(job.data);
        }
        return 0;
    }
    if (!pool->started) {
        sql_set_error(error, 0, 0, "thread pool is not started");
        if (job.cleanup != NULL) {
            job.cleanup(job.data);
        }
        return 0;
    }
    if (job.execute == NULL) {
        sql_set_error(error, 0, 0, "job execute callback must not be null");
        if (job.cleanup != NULL) {
            job.cleanup(job.data);
        }
        return 0;
    }

    if (!job_queue_push(pool->queue, job, error)) {
        if (job.cleanup != NULL) {
            job.cleanup(job.data);
        }
        return 0;
    }

    return 1;
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
 * 구현 상태:
 * - queue를 닫고 worker join을 완료한다.
 */
void thread_pool_stop(ThreadPool *pool) {
    int index = 0;

    if (pool == NULL) {
        return;
    }

    job_queue_close(pool->queue);
    if (!pool->started) {
        return;
    }

    for (index = 0; index < pool->worker_count; index++) {
        pthread_join(pool->workers[index], NULL);
    }
    pool->started = 0;
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
 * 구현 상태:
 * - stop 이후 queue와 worker 배열을 해제한다.
 */
void thread_pool_destroy(ThreadPool *pool) {
    if (pool == NULL) {
        return;
    }

    thread_pool_stop(pool);
    job_queue_destroy(pool->queue);
    free(pool->workers);
    free(pool);
}
