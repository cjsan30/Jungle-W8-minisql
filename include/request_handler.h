#ifndef REQUEST_HANDLER_H
#define REQUEST_HANDLER_H

#include "common.h"
#include "db_guard.h"
#include "http_protocol.h"
#include "sql_service.h"

typedef struct {
    SqlService *sql_service;
    DbGuard *db_guard;
} RequestHandler;

/*
 * 기능:
 * - 요청 처리기를 생성한다.
 * - HTTP 요청에서 SQL을 꺼내 DB 서비스로 전달하는 오케스트레이션 레이어다.
 *
 * 반환값:
 * - 성공: 초기화된 RequestHandler 포인터
 * - 실패: NULL, error에 실패 원인 기록
 */
RequestHandler *request_handler_create(SqlService *sql_service, DbGuard *db_guard, SqlError *error);

/*
 * 기능:
 * - 단일 요청을 처리해 응답을 만든다.
 * - 외부 요청 -> SQL 실행 -> HTTP 응답 변환의 핵심 흐름이다.
 *
 * 반환값:
 * - 성공: 1
 * - 실패: 0, error에 실패 원인 기록
 *
 * 흐름:
 * - 요청 유효성을 검사한다.
 * - SQL 실행 결과를 받는다.
 * - 성공/실패에 맞는 응답 payload를 구성한다.
 */
int request_handler_handle(RequestHandler *handler,
                           const HttpRequest *request,
                           HttpResponse *response,
                           SqlError *error);

/*
 * 기능:
 * - 요청 처리기를 해제한다.
 *
 * 반환값:
 * - 없음
 */
void request_handler_destroy(RequestHandler *handler);

#endif
