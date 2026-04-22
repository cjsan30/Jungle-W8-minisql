#ifndef SERVER_JOB_H
#define SERVER_JOB_H

#include "common.h"
#include "job_queue.h"

typedef struct {
    int client_fd;
    char *raw_request;
    size_t raw_request_length;
} ServerJobData;

/*
 * 기능:
 * - API 서버가 worker queue에 넘길 공용 작업 payload를 초기화한다.
 * - 1번과 2번이 공유하는 최소 전달 계약이다.
 *
 * 반환값:
 * - 없음
 *
 * 흐름:
 * - client fd와 raw request 정보를 구조체에 채운다.
 * - worker가 소비할 때 필요한 최소 입력만 유지한다.
 */
void server_job_data_init(ServerJobData *job_data,
                          int client_fd,
                          char *raw_request,
                          size_t raw_request_length);

/*
 * 기능:
 * - 공용 작업 payload의 유효성을 검사한다.
 *
 * 반환값:
 * - 성공: 1
 * - 실패: 0, error에 원인 기록
 *
 * 흐름:
 * - client fd가 유효한지 검사한다.
 * - request 버퍼와 길이가 일관적인지 검사한다.
 */
int server_job_data_validate(const ServerJobData *job_data, SqlError *error);

/*
 * 기능:
 * - thread pool에 제출할 Job 구조체를 만든다.
 * - worker 실행 함수와 cleanup 함수를 하나의 계약으로 묶는다.
 *
 * 반환값:
 * - 성공: Job 구조체
 *
 * 흐름:
 * - execute/cleanup/data 포인터를 채운다.
 * - 1번은 이 함수를 통해 Job을 만들고, 2번은 queue에서 동일한 형태로 받는다.
 */
Job server_job_build(JobExecuteFn execute, JobCleanupFn cleanup, void *job_data);

/*
 * 기능:
 * - 공용 작업 payload 내부 메모리를 정리한다.
 *
 * 반환값:
 * - 없음
 *
 * 흐름:
 * - raw request 버퍼를 해제한다.
 * - payload 상태를 초기화한다.
 */
void server_job_data_destroy(ServerJobData *job_data);

#endif
