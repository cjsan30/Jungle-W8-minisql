#include "server_job.h"

#include <unistd.h>

/*
 * 기능:
 * - API 서버가 queue에 넘길 공용 작업 payload를 채운다.
 *
 * 반환값:
 * - 없음
 *
 * 흐름:
 * - client fd와 raw request 정보를 구조체에 연결한다.
 * - 메모리 소유권은 cleanup 단계에서 정리하는 것을 전제로 한다.
 *
 * 구현 상태:
 * - 협업용 공용 payload를 초기화한다.
 */
void server_job_data_init(ServerJobData *job_data,
                          int client_fd,
                          char *raw_request,
                          size_t raw_request_length) {
    if (job_data == NULL) {
        return;
    }

    job_data->client_fd = client_fd;
    job_data->raw_request = raw_request;
    job_data->raw_request_length = raw_request_length;
}

/*
 * 기능:
 * - 공용 작업 payload의 최소 유효성을 검사한다.
 *
 * 반환값:
 * - 성공: 1
 * - 실패: 0, error에 원인 기록
 *
 * 흐름:
 * - fd, request 포인터, 길이를 검사한다.
 *
 * 구현 상태:
 * - 협업용 공용 payload를 검증한다.
 */
int server_job_data_validate(const ServerJobData *job_data, SqlError *error) {
    if (job_data == NULL) {
        sql_set_error(error, 0, 0, "server_job_data must not be null");
        return 0;
    }
    if (job_data->client_fd < 0) {
        sql_set_error(error, 0, 0, "server_job_data client_fd must be valid");
        return 0;
    }
    if (job_data->raw_request == NULL || job_data->raw_request_length == 0U) {
        sql_set_error(error, 0, 0, "server_job_data raw_request must not be empty");
        return 0;
    }
    return 1;
}

/*
 * 기능:
 * - thread pool 제출용 Job 구조체를 만든다.
 *
 * 반환값:
 * - 성공: Job 구조체
 *
 * 흐름:
 * - execute, cleanup, data를 채운다.
 *
 * 구현 상태:
 * - thread pool 제출용 Job 값을 만든다.
 */
Job server_job_build(JobExecuteFn execute, JobCleanupFn cleanup, void *job_data) {
    Job job = {0};

    job.execute = execute;
    job.cleanup = cleanup;
    job.data = job_data;
    return job;
}

/*
 * 기능:
 * - 공용 작업 payload 내부 메모리를 정리한다.
 *
 * 반환값:
 * - 없음
 *
 * 흐름:
 * - raw request 버퍼를 해제한다.
 * - 필드를 초기화한다.
 *
 * 구현 상태:
 * - worker cleanup 단계에서 fd와 request 버퍼를 정리한다.
 */
void server_job_data_destroy(ServerJobData *job_data) {
    if (job_data == NULL) {
        return;
    }

    if (job_data->client_fd >= 0) {
        close(job_data->client_fd);
    }
    free(job_data->raw_request);
    job_data->raw_request = NULL;
    job_data->raw_request_length = 0U;
    job_data->client_fd = -1;
}

void server_job_cleanup(void *job_data) {
    ServerJobData *server_job_data = job_data;

    server_job_data_destroy(server_job_data);
    free(server_job_data);
}
