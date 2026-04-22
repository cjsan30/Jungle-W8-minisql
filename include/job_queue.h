#ifndef JOB_QUEUE_H
#define JOB_QUEUE_H

#include "common.h"

typedef void (*JobExecuteFn)(void *job_data);
typedef void (*JobCleanupFn)(void *job_data);

typedef struct {
    JobExecuteFn execute;
    JobCleanupFn cleanup;
    void *data;
} Job;

typedef struct JobQueue JobQueue;

/*
 * 기능:
 * - worker thread가 소비할 작업 큐를 생성한다.
 *
 * 반환값:
 * - 성공: JobQueue 포인터
 * - 실패: NULL, error에 실패 원인 기록
 */
JobQueue *job_queue_create(int capacity, SqlError *error);

/*
 * 기능:
 * - 큐에 작업을 넣는다.
 *
 * 반환값:
 * - 성공: 1
 * - 실패: 0, error에 실패 원인 기록
 *
 * 흐름:
 * - 큐 종료 여부를 확인한다.
 * - 공간이 없으면 pop 또는 close가 발생할 때까지 대기한다.
 * - 작업을 push한다.
 *
 * 계약:
 * - push 실패 시 Job.data cleanup은 호출자가 책임진다.
 */
int job_queue_push(JobQueue *queue, Job job, SqlError *error);

/*
 * 기능:
 * - 큐에서 다음 작업을 꺼낸다.
 *
 * 반환값:
 * - 성공: 1
 * - 닫힌 큐가 비었거나 입력 오류: 0
 *
 * 계약:
 * - close 이후에도 이미 들어간 작업은 모두 pop된다.
 */
int job_queue_pop(JobQueue *queue, Job *out_job);

/*
 * 기능:
 * - 더 이상 새 작업을 받지 않도록 큐를 닫는다.
 *
 * 반환값:
 * - 없음
 */
void job_queue_close(JobQueue *queue);

/*
 * 기능:
 * - 큐 메모리를 해제한다.
 *
 * 반환값:
 * - 없음
 */
void job_queue_destroy(JobQueue *queue);

#endif
