#include "job_queue.h"

struct JobQueue {
    int capacity;
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
 * 현재 상태:
 * - queue 생성을 위한 stub 함수다.
 */
JobQueue *job_queue_create(int capacity, SqlError *error) {
    (void) capacity;
    sql_set_error(error, 0, 0, "job_queue_create stub: 작업 큐 생성 로직이 아직 구현되지 않았습니다");
    return NULL;
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
 * 현재 상태:
 * - enqueue를 위한 stub 함수다.
 */
int job_queue_push(JobQueue *queue, Job job, SqlError *error) {
    (void) queue;
    (void) job;
    sql_set_error(error, 0, 0, "job_queue_push stub: producer enqueue 로직이 아직 구현되지 않았습니다");
    return 0;
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
 * 현재 상태:
 * - dequeue를 위한 stub 함수다.
 */
int job_queue_pop(JobQueue *queue, Job *out_job) {
    (void) queue;
    (void) out_job;
    return 0;
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
 * 현재 상태:
 * - 종료 지점을 위한 stub 함수다.
 */
void job_queue_close(JobQueue *queue) {
    (void) queue;
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
 * 현재 상태:
 * - 해제 지점을 위한 stub 함수다.
 */
void job_queue_destroy(JobQueue *queue) {
    (void) queue;
}
