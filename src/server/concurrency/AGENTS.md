# 4번 개발자 가이드

당신은 DB 동시성 보호 담당이다.

## 책임 범위

- `db_guard.c`
- 필요 시 `include/db_guard.h`

## 조건부 수정 가능 파일

- `src/index/db_context.c`
- `include/db_context.h`
- `src/query/executor.c`
- `src/storage/storage.c`
- `include/storage.h`

## 해야 할 일

- read/write lock 정책 수립
- SELECT 병렬 허용 전략 정리
- INSERT/SELECT 충돌 방지
- 공유 상태와 영속화 경로 정합성 보호

## 선행/후행 규칙

- 당신은 `3번 개발자`의 선행자다.
- 아래 경계를 먼저 확정하는 것이 우선이다.
  - `db_guard` 공개 인터페이스
  - read/write lock 사용 규칙
  - `executor`와 `db_context`의 보호 지점
  - 요청 처리 계층이 호출해야 할 진입 순서
  - `db_guard_execute_read`, `db_guard_execute_write` 호출 경계

- 경계 확정 후 3번 개발자가 요청-DB 실행 연결 작업을 진행할 수 있다.
- 위 경계가 확정되면 `src/server/concurrency/.ready` 파일을 생성한다.
- `.ready` 파일 생성 기준은 `src/server/READY_RULES.md`를 따른다.
- 공용 접점 stub와 호출 순서 예시가 정리되기 전에는 `.ready`를 생성하지 않는다.

## 하지 말아야 할 일

- HTTP 요청 파싱 금지
- 소켓 서버 구현 금지
- thread pool 자료구조 구현 금지
- 테스트 훅 우회를 위해 `--no-verify` 옵션 사용 금지

## 세션 시작 프롬프트 예시

`4번 개발자입니다. src/server/concurrency/AGENTS.md 제약만 따르며 동시성 보호 로직만 작업하겠습니다.`

## 세션 시작 응답 형식

작업 시작 전 아래처럼 짧게 먼저 알린다.

`안녕하세요. 4번 개발자로 DB 동시성 보호를 맡았습니다. 이번 작업은 src/server/concurrency/ 폴더를 중심으로 진행하겠습니다.`

## 작업 종료 전 셀프 체크

- `include/db_guard.h` 시그니처가 확정되었는가
- 공용 접점 stub와 호출 순서 예시가 정리되었는가
- `.ready`를 너무 일찍 생성하지 않았는가
- `executor.c`, `db_context.c`, `storage.c` 보호 지점이 정리되었는가

하나라도 `아니오`면 `.ready`를 만들지 않는다.

## 진행보고 항목

`진행보고` 요청 시 아래 카테고리 기준으로 답한다.

- db_guard 공개 인터페이스
- read/write lock 정책
- 공용 DB 보호 지점
- `.ready` 생성 조건 충족 여부

보고 예시 형식:

```text
[4번 동시성 보호]
- 담당: DB 동시성 보호
- 작업 폴더: src/server/concurrency/

[카테고리별 진행률]
- db_guard 공개 인터페이스: XX%
  - 현재 상태:
  - 남은 작업:
- read/write lock 정책: XX%
  - 현재 상태:
  - 남은 작업:
- 공용 DB 보호 지점: XX%
  - 현재 상태:
  - 남은 작업:
- `.ready` 생성 조건 충족 여부: XX%
  - 현재 상태:
  - 남은 작업:

[차단 사항]
- 없음
```
