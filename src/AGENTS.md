# Source Workspace Guide

이 프로젝트의 로직 작업은 `src/` 아래에서 진행한다.

## 역할 라우팅 규칙

세션에서 사용자가 `1번 개발자`, `2번 개발자`, `3번 개발자`, `4번 개발자`처럼 역할 번호를 말하면 아래 순서로 행동한다.

1. 이 `src/AGENTS.md`를 먼저 읽는다.
2. 역할 번호에 대응하는 작업 폴더를 찾는다.
3. 해당 폴더의 `AGENTS.md`를 추가로 읽는다.
4. 그 `AGENTS.md`에 적힌 수정 가능 범위 안에서만 작업한다.
5. 작업 시작 전에 가벼운 인사와 함께 자신이 맡은 역할, 작업 폴더를 1~2문장으로 먼저 설명한다.
6. 그 다음 읽은 `AGENTS.md` 요약과 수정 예정 파일 목록을 출력한다.
7. 역할 범위 밖 파일은 수정하지 않는다.

응답 형식 예시:

- `안녕하세요. 3번 개발자로 요청 처리와 DB 실행 어댑터를 맡았습니다. 이번 작업은 src/server/request/ 폴더를 중심으로 진행하겠습니다.`

## 진행보고 규칙

사용자가 `진행보고`라고 요청하면 아래 순서로 답한다.

1. 현재 자신이 맡은 역할과 담당 폴더를 한 줄로 다시 밝힌다.
2. 담당 역할의 요구사항을 카테고리별로 나눈다.
3. 각 카테고리마다 현재 완료율을 `%`로 표시한다.
4. 각 카테고리마다 남은 작업을 짧게 적는다.
5. 선행/후행 관계로 인해 막혀 있다면, 무엇이 아직 없어 진행이 멈췄는지 적는다.

공통 보고 형식:

```text
[역할]
- 담당:
- 작업 폴더:

[카테고리별 진행률]
- 카테고리명: XX%
  - 현재 상태:
  - 남은 작업:

[차단 사항]
- 없음
```

## 선행/후행 작업 규칙

- `2번 개발자`가 `1번 개발자`보다 선행한다.
- `4번 개발자`가 `3번 개발자`보다 선행한다.
- `1번 개발자`는 `src/server/pool/.ready` 파일이 있어야 공용 접점 연결 작업을 계속 진행할 수 있다.
- `3번 개발자`는 `src/server/concurrency/.ready` 파일이 있어야 공용 접점 연결 작업을 계속 진행할 수 있다.
- `2번 개발자`는 스레드풀 공개 인터페이스와 최소 동작 경계를 확정하면 `src/server/pool/.ready` 파일을 생성한다.
- `4번 개발자`는 DB 보호 계층과 공용 DB 접점을 확정하면 `src/server/concurrency/.ready` 파일을 생성한다.
- `1번 개발자`는 `2번 개발자`의 스레드풀 공개 인터페이스가 확정되기 전까지 `src/server/api/` 내부의 안전 범위만 작업한다.
- `3번 개발자`는 `4번 개발자`의 DB 보호 계층과 공용 DB 접점이 확정되기 전까지 `src/server/request/` 내부의 안전 범위만 작업한다.
- 후행자는 선행자 작업이 완료되지 않았더라도 완전 대기하지 않는다. 다만 공용 접점 연결은 보류한다.
- 후행자의 안전 범위가 끝나면, 왜 추가 작업이 위험한지와 어떤 선행 작업 완료가 필요한지를 짧게 설명한다.

## .ready 확인 규칙

- 후행자는 작업 시작 시 먼저 아래 파일 존재 여부를 확인한다.
  - `1번 개발자` -> `src/server/pool/.ready`
  - `3번 개발자` -> `src/server/concurrency/.ready`
- `.ready`가 없으면 안전 범위까지만 작업한다.
- `.ready`가 있으면 공용 접점 연결 작업까지 진행할 수 있다.
- `.ready` 파일의 의미와 생성 기준은 `src/server/READY_RULES.md`를 따른다.

예시:

- `1번 개발자`: `thread_pool_submit`, worker lifecycle, queue 인터페이스가 아직 확정되지 않아 서버-스레드풀 연결 코드를 진행할 수 없습니다. 2번 개발자의 공개 함수 시그니처 확정 후 이어서 작업하겠습니다.`
- `3번 개발자`: `db_guard`, executor 진입 방식, DB 락 정책이 아직 확정되지 않아 요청-DB 실행 연결을 더 진행할 수 없습니다. 4번 개발자의 공용 인터페이스 확정 후 이어서 작업하겠습니다.`

## 협업 표준 계약

### 1번-2번 계약

- 선행자: `2번 개발자`
- 후행자: `1번 개발자`
- `2번 개발자`는 아래 공개 인터페이스를 먼저 확정한다.
  - `thread_pool_create`
  - `thread_pool_start`
  - `thread_pool_submit`
  - `thread_pool_stop`
  - `job_queue_push`
  - `job_queue_pop`
- 추가로 아래 계약을 먼저 고정한다.
  - `Job.data` 구조체 타입
  - 소켓 fd 종료 책임
  - 작업 실패 시 cleanup 책임
- `1번 개발자`는 `thread_pool_submit()` 호출만 담당하고 queue 내부 구현은 맡지 않는다.
- `2번 개발자`는 queue/worker 내부 구현만 책임진다.
- `Job.data` 해제 책임은 worker 쪽으로 통일한다.

### 3번-4번 계약

- 선행자: `4번 개발자`
- 후행자: `3번 개발자`
- `4번 개발자`는 아래 공개 인터페이스를 먼저 확정한다.
  - `db_guard_create`
  - `db_guard_lock_read`
  - `db_guard_lock_write`
  - `db_guard_unlock_read`
  - `db_guard_unlock_write`
  - `db_guard_destroy`
- 추가로 아래 계약을 먼저 고정한다.
  - `SELECT`는 read lock
  - `INSERT`는 write lock
  - 락 획득/해제 책임 위치
  - `sql_service_execute()` 안에서 락을 처리할지 여부
- `3번 개발자`는 락 정책을 직접 만들지 않고 확정된 인터페이스만 호출한다.
- `4번 개발자`는 `executor.c`, `db_context.c`, `storage.c` 보호 지점을 우선 소유한다.

### 공용 위험 파일 소유권

- `Makefile`: 통합 담당 1명만 수정
- `src/query/executor.c`: `4번 개발자` 우선 소유
- `src/index/db_context.c`: `4번 개발자` 우선 소유
- `src/storage/storage.c`: `4번 개발자` 우선 소유
- `src/runtime/main.c`: `3번 개발자` 필요 시만 수정, 수정 전 공유
- 공용 헤더 소유권
  - `include/server.h`, `include/server_config.h`: `1번 개발자`
  - `include/thread_pool.h`, `include/job_queue.h`: `2번 개발자`
  - `include/http_protocol.h`, `include/request_handler.h`, `include/sql_service.h`: `3번 개발자`
  - `include/db_guard.h`: `4번 개발자`

### stub 전달 규칙

- 공용 접점 stub는 최초에 소유자가 생성해 전체 팀에 전달한다.
- 후행자는 제공된 stub와 공개 헤더를 기준으로만 연결 코드를 작성한다.
- 후행자는 공용 접점 stub를 임의로 확장하거나 이름을 바꾸지 않는다.
- stub 변경이 필요하면 소유자에게 변경 요청을 보낸다.
- 현재 공용 stub 기준
  - `1번-2번`: `include/server_job.h`, `src/server/pool/server_job_stub.c`
  - `3번-4번`: `include/sql_service.h`의 `sql_service_classify_operation`, `include/db_guard.h`의 `db_guard_execute_read`, `db_guard_execute_write`

## 작업 원칙

- 새 서버 로직은 `src/server/` 하위 역할별 폴더에 추가한다.
- 기존 DB 엔진 수정은 필요한 경우에만 `src/query/`, `src/index/`, `src/storage/`, `src/runtime/`에서 진행한다.
- 세션 시작 시 반드시 자신의 역할을 명시한다.
  - 예시: `3번 개발자입니다. src/server/request/AGENTS.md 제약만 따르겠습니다.`
- 본인 역할 폴더의 `AGENTS.md`를 우선 규칙으로 따른다.

## 커밋 및 푸시 규칙

- `git commit --no-verify` 사용 금지
- `git push --no-verify` 사용 금지
- 커밋과 푸시 전 실행되는 테스트 훅을 우회하지 않는다
- 테스트가 실패하면 원인을 수정한 뒤 다시 시도한다

## 작업 종료 전 셀프 체크

작업을 마치기 전 아래를 확인한다.

- 내가 소유하지 않은 공용 구현파일을 수정했는가
- 내가 소유하지 않은 공용 헤더를 수정했는가
- `.ready` 이후 공개 시그니처를 바꿨는가
- 공용 접점 stub를 임의로 바꿨는가

하나라도 `예`면 바로 커밋하거나 푸시하지 말고 먼저 정리하거나 소유자와 조율한다.

## 역할 폴더

- `src/server/api/`: 외부 클라이언트 연결, 서버 생명주기
- `src/server/pool/`: 스레드풀, 작업 큐
- `src/server/request/`: 요청 파싱, SQL 실행 연결
- `src/server/concurrency/`: DB 동시성 보호

## 역할별 작업 폴더와 하위 가이드

- `1번 개발자`
  - 작업 폴더: `src/server/api/`
  - 추가로 읽을 파일: `src/server/api/AGENTS.md`

- `2번 개발자`
  - 작업 폴더: `src/server/pool/`
  - 추가로 읽을 파일: `src/server/pool/AGENTS.md`

- `3번 개발자`
  - 작업 폴더: `src/server/request/`
  - 추가로 읽을 파일: `src/server/request/AGENTS.md`

- `4번 개발자`
  - 작업 폴더: `src/server/concurrency/`
  - 추가로 읽을 파일: `src/server/concurrency/AGENTS.md`

## 공용 위험 파일

아래 파일은 역할 경계 밖 수정이 필요할 수 있으니, 수정 전 팀 내 합의가 필요하다.

- `Makefile`
- `src/runtime/main.c`
- `src/query/executor.c`
- `src/index/db_context.c`
- `src/storage/storage.c`
