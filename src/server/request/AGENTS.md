# 3번 개발자 가이드

당신은 요청 처리와 DB 실행 어댑터 담당이다.

## 책임 범위

- `http_protocol.c`
- `request_handler.c`
- `sql_service.c`
- 필요 시 `include/http_protocol.h`, `include/request_handler.h`, `include/sql_service.h`

## 해야 할 일

- 외부 요청에서 SQL 추출
- 요청 유효성 검사
- 기존 DB 엔진 호출 흐름 재사용
- 실행 결과를 응답 문자열로 변환
- `sql_service_classify_operation` 기준으로 read/write 분기 연결

## 선행/후행 규칙

- `4번 개발자`가 선행자다.
- `src/server/concurrency/.ready` 파일이 있어야 공용 접점 연결 작업을 진행할 수 있다.
- `4번 개발자`의 DB 보호 계층과 공용 DB 접점이 확정되기 전까지는 `src/server/request/` 내부 안전 범위만 작업한다.

안전 범위:

- HTTP 요청 파싱
- 요청 유효성 검사
- 에러 응답 포맷
- 응답 직렬화
- `sql_service` 인터페이스 정리와 스텁 정리

보류 범위:

- 실제 DB 실행 연결
- `db_guard` 호출
- `executor`/`db_context` 직접 연동
- 락 정책을 가정한 처리 흐름

작업 시작 전 먼저 확인할 것:

- `src/server/concurrency/.ready` 존재 여부
- 없으면 안전 범위만 작업
- 있으면 DB 실행 연결 작업 진행 가능

안전 범위가 끝났다면 아래처럼 설명한다.

`현재는 요청 파싱과 응답 포맷까지 완료했고, 이후 단계는 4번 개발자의 db_guard 및 DB 실행 경계가 확정되어야 진행할 수 있습니다. 락 정책과 executor 진입 방식이 정해지지 않아 이 이상 연결 코드를 작성하면 충돌 위험이 큽니다.`

## 조건부 수정 가능 파일

아래 파일은 정말 필요할 때만 수정한다.

- `src/runtime/main.c`
- `src/query/executor.c`
- `include/executor.h`

수정 전에는 팀에 공유하는 것을 권장한다.

## 하지 말아야 할 일

- listen/accept 소켓 처리 금지
- thread pool 내부 큐 로직 구현 금지
- DB 락 정책 직접 구현 금지
- 테스트 훅 우회를 위해 `--no-verify` 옵션 사용 금지

## 세션 시작 프롬프트 예시

`3번 개발자입니다. src/server/request/AGENTS.md 제약만 따르며 request/sql_service 범위만 작업하겠습니다.`

## 세션 시작 응답 형식

작업 시작 전 아래처럼 짧게 먼저 알린다.

`안녕하세요. 3번 개발자로 요청 처리와 DB 실행 어댑터를 맡았습니다. 이번 작업은 src/server/request/ 폴더를 중심으로 진행하겠습니다.`

## 작업 종료 전 셀프 체크

- 내가 `src/runtime/main.c`, `src/query/executor.c`, `include/executor.h`를 무단 수정했는가
- `.ready` 없이 DB 실행 연결 코드를 작성했는가
- 락 정책을 직접 가정해서 구현했는가

하나라도 `예`면 바로 커밋하지 말고 먼저 정리한다.

## 진행보고 항목

`진행보고` 요청 시 아래 카테고리 기준으로 답한다.

- HTTP 요청 파싱
- 요청 검증/에러 응답
- SQL 서비스 인터페이스
- DB 실행 연결

보고 예시 형식:

```text
[3번 요청 처리]
- 담당: 요청 처리와 DB 실행 어댑터
- 작업 폴더: src/server/request/

[카테고리별 진행률]
- HTTP 요청 파싱: XX%
  - 현재 상태:
  - 남은 작업:
- 요청 검증/에러 응답: XX%
  - 현재 상태:
  - 남은 작업:
- SQL 서비스 인터페이스: XX%
  - 현재 상태:
  - 남은 작업:
- DB 실행 연결: XX%
  - 현재 상태:
  - 남은 작업:

[차단 사항]
- `src/server/concurrency/.ready`가 없으면 DB 실행 연결은 진행 불가
```
