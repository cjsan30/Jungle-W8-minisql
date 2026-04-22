#include "job_queue.h"

#include <pthread.h>

struct JobQueue {
    int capacity;
    int count;
    int head;
    int tail;
    int closed;
    Job *jobs;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
};

/*
 * 기능:
 * - worker가 소비할 작업 큐를 생성한다.
 *
 * 반환값:
 * - 성공: `JobQueue *`
 * - 실패: `NULL`, error에 원인 기록
 *
 * 흐름:
 * - queue 크기를 검증한다.
 * - 내부 버퍼와 상태값을 준비한다.
 *
 * 구현 상태:
 * - bounded queue 버퍼와 동기화 객체를 생성한다.
 */
JobQueue *job_queue_create(int capacity, SqlError *error) {
    JobQueue *queue = NULL;

    if (capacity <= 0) {
        sql_set_error(error, 0, 0, "job queue capacity must be positive");
        return NULL;
    }

    queue = calloc(1U, sizeof(*queue));
    if (queue == NULL) {
        sql_set_error(error, 0, 0, "failed to allocate job queue");
        return NULL;
    }

    queue->jobs = calloc((size_t) capacity, sizeof(*queue->jobs));
    if (queue->jobs == NULL) {
        sql_set_error(error, 0, 0, "failed to allocate job queue buffer");
        free(queue);
        return NULL;
    }

    queue->capacity = capacity;
    if (pthread_mutex_init(&queue->mutex, NULL) != 0) {
        sql_set_error(error, 0, 0, "failed to initialize job queue mutex");
        free(queue->jobs);
        free(queue);
        return NULL;
    }
    if (pthread_cond_init(&queue->not_empty, NULL) != 0) {
        sql_set_error(error, 0, 0, "failed to initialize job queue not_empty condition");
        pthread_mutex_destroy(&queue->mutex);
        free(queue->jobs);
        free(queue);
        return NULL;
    }
    if (pthread_cond_init(&queue->not_full, NULL) != 0) {
        sql_set_error(error, 0, 0, "failed to initialize job queue not_full condition");
        pthread_cond_destroy(&queue->not_empty);
        pthread_mutex_destroy(&queue->mutex);
        free(queue->jobs);
        free(queue);
        return NULL;
    }

    return queue;
}

/*
 * 기능:
 * - 작업 1건을 queue에 넣는다.
 *
 * 반환값:
 * - 성공: 1
 * - 실패: 0, error에 원인 기록
 *
 * 흐름:
 * - queue 종료 여부를 확인한다.
 * - 여유 공간이 있으면 작업을 저장한다.
 * - 대기 중인 consumer를 깨운다.
 *
 * 구현 상태:
 * - queue에 공간이 생길 때까지 대기한 뒤 enqueue한다.
 */
int job_queue_push(JobQueue *queue, Job job, SqlError *error) {
    if (queue == NULL) {
        sql_set_error(error, 0, 0, "job queue must not be null");
        return 0;
    }
    if (job.execute == NULL) {
        sql_set_error(error, 0, 0, "job execute callback must not be null");
        return 0;
    }

    pthread_mutex_lock(&queue->mutex);
    while (!queue->closed && queue->count == queue->capacity) {
        pthread_cond_wait(&queue->not_full, &queue->mutex);
    }

    if (queue->closed) {
        pthread_mutex_unlock(&queue->mutex);
        sql_set_error(error, 0, 0, "job queue is closed");
        return 0;
    }

    queue->jobs[queue->tail] = job;
    queue->tail = (queue->tail + 1) % queue->capacity;
    queue->count++;
    pthread_cond_signal(&queue->not_empty);
    pthread_mutex_unlock(&queue->mutex);
    return 1;
}

/*
 * 기능:
 * - queue에서 다음 작업을 꺼낸다.
 *
 * 반환값:
 * - 성공: 1
 * - 실패 또는 종료: 0
 *
 * 흐름:
 * - queue가 비었는지 확인한다.
 * - 작업이 있으면 하나 꺼내 `out_job`에 채운다.
 *
 * 구현 상태:
 * - 작업이 들어오거나 queue가 닫힐 때까지 대기한 뒤 dequeue한다.
 */
int job_queue_pop(JobQueue *queue, Job *out_job) {
    if (queue == NULL || out_job == NULL) {
        return 0;
    }

    pthread_mutex_lock(&queue->mutex);
    while (!queue->closed && queue->count == 0) {
        pthread_cond_wait(&queue->not_empty, &queue->mutex);
    }

    if (queue->count == 0) {
        pthread_mutex_unlock(&queue->mutex);
        return 0;
    }

    *out_job = queue->jobs[queue->head];
    memset(&queue->jobs[queue->head], 0, sizeof(queue->jobs[queue->head]));
    queue->head = (queue->head + 1) % queue->capacity;
    queue->count--;
    pthread_cond_signal(&queue->not_full);
    pthread_mutex_unlock(&queue->mutex);
    return 1;
}

/*
 * 기능:
 * - queue를 닫아 새 작업 수신을 멈춘다.
 *
 * 반환값:
 * - 없음
 *
 * 흐름:
 * - 종료 상태로 전환한다.
 * - 대기 중인 worker를 깨운다.
 *
 * 구현 상태:
 * - queue를 closed 상태로 바꾸고 대기 중인 producer/consumer를 깨운다.
 */
void job_queue_close(JobQueue *queue) {
    if (queue == NULL) {
        return;
    }

    pthread_mutex_lock(&queue->mutex);
    queue->closed = 1;
    pthread_cond_broadcast(&queue->not_empty);
    pthread_cond_broadcast(&queue->not_full);
    pthread_mutex_unlock(&queue->mutex);
}

/*
 * 기능:
 * - queue 메모리를 해제한다.
 *
 * 반환값:
 * - 없음
 *
 * 흐름:
 * - 내부 버퍼와 상태 자원을 정리한다.
 * - 마지막에 queue 구조체를 해제한다.
 *
 * 구현 상태:
 * - 남은 작업 cleanup 후 queue 자원을 해제한다.
 */
void job_queue_destroy(JobQueue *queue) {
    int index = 0;

    if (queue == NULL) {
        return;
    }

    job_queue_close(queue);
    pthread_mutex_lock(&queue->mutex);
    for (index = 0; index < queue->count; index++) {
        Job *job = &queue->jobs[(queue->head + index) % queue->capacity];
        if (job->cleanup != NULL) {
            job->cleanup(job->data);
        }
    }
    pthread_mutex_unlock(&queue->mutex);

    pthread_cond_destroy(&queue->not_full);
    pthread_cond_destroy(&queue->not_empty);
    pthread_mutex_destroy(&queue->mutex);
    free(queue->jobs);
    free(queue);
}
