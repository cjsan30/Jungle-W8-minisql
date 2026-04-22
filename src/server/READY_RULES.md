# Ready Marker Rules

이 문서는 선행/후행 작업에서 사용하는 `.ready` 마커 파일의 의미를 정의한다.

## 목적

후행 개발자가 공용 접점 연결 작업을 시작해도 되는 시점을 파일 존재 여부로 빠르게 판단할 수 있게 한다.

중요:

- `.ready`는 "모든 구현이 100% 최종 완료되었다"는 뜻이 아니다.
- `.ready`는 "후행 개발자가 붙어도 공개 인터페이스와 연결 경계가 더 이상 크게 흔들리지 않는다"는 뜻이다.
- 즉, 후행자 작업 시작 가능 상태를 의미한다.
- `.ready`는 선행자의 주관적 판단으로 만들지 않는다.
- 아래 체크리스트를 만족해야만 생성한다.

## 마커 파일

- `src/server/pool/.ready`
  - 생성자: `2번 개발자`
  - 의미: `1번 개발자`가 thread pool과 queue 연결 코드를 붙여도 될 만큼 공개 인터페이스와 최소 동작 경계가 안정화되었다.

- `src/server/concurrency/.ready`
  - 생성자: `4번 개발자`
  - 의미: `3번 개발자`가 request 계층에서 DB 실행 연결 코드를 붙여도 될 만큼 DB 보호 계층과 공용 DB 접점이 안정화되었다.

## 생성 기준

### `src/server/pool/.ready`

아래가 정해졌을 때 생성한다.

- `thread_pool_create`
- `thread_pool_start`
- `thread_pool_submit`
- `thread_pool_stop`
- `job_queue_push`
- `job_queue_pop`
- worker 종료와 queue 종료의 기본 정책
- `Job.data` 구조체 타입
- 소켓 fd 종료 책임
- 작업 실패 시 cleanup 책임

아래 상태까지 확인된 뒤 생성하는 것을 권장한다.

- 헤더 시그니처가 더 이상 바뀌지 않음
- 최소 1회 submit -> worker 실행 경로가 동작함
- 후행자인 `1번 개발자`가 연결 코드를 붙여도 재작업 가능성이 낮음

필수 체크리스트:

- [ ] `include/thread_pool.h`, `include/job_queue.h` 시그니처 확정
- [ ] 공용 접점 stub와 사용 예시 확정
- [ ] `Job.data` 구조와 메모리 해제 책임 문서화
- [ ] queue 종료/worker 종료 정책 문서화
- [ ] 최소 1회 submit -> worker 실행 경로 확인

위 항목 중 하나라도 미완료면 `.ready`를 생성하지 않는다.

### `src/server/concurrency/.ready`

아래가 정해졌을 때 생성한다.

- `db_guard` 공개 인터페이스
- read/write lock 정책
- `executor` 보호 지점
- `db_context` 보호 지점
- request 계층이 DB 실행 전후에 호출해야 할 순서
- `SELECT`/`INSERT` 판별과 락 책임 위치

아래 상태까지 확인된 뒤 생성하는 것을 권장한다.

- 헤더 시그니처가 더 이상 바뀌지 않음
- lock/unlock 기본 경로가 정리됨
- 후행자인 `3번 개발자`가 연결 코드를 붙여도 재작업 가능성이 낮음

필수 체크리스트:

- [ ] `include/db_guard.h` 시그니처 확정
- [ ] 공용 접점 stub와 호출 순서 예시 확정
- [ ] read/write lock 정책 문서화
- [ ] `SELECT`/`INSERT` 판별과 락 책임 위치 확정
- [ ] lock/unlock 기본 경로 확인

위 항목 중 하나라도 미완료면 `.ready`를 생성하지 않는다.

## 후행자 규칙

- `1번 개발자`는 `src/server/pool/.ready`가 없으면 안전 범위만 작업한다.
- `3번 개발자`는 `src/server/concurrency/.ready`가 없으면 안전 범위만 작업한다.
- `.ready`가 생기기 전 공용 접점 연결 코드를 임의로 작성하지 않는다.
- `.ready`가 있다고 해서 선행자의 모든 구현이 완전히 끝났다는 뜻은 아니다.
- `.ready` 이후에도 내부 구현 보완은 가능하지만, 공개 인터페이스와 연결 경계는 가급적 바꾸지 않는다.
- `.ready` 생성 후 공개 인터페이스를 바꾸면 안 된다. 꼭 바꿔야 하면 `.ready`를 제거하고 후행자에게 즉시 알린다.

## 마커 생성 예시

Linux/macOS/WSL:

```bash
touch src/server/pool/.ready
touch src/server/concurrency/.ready
```

Windows PowerShell:

```powershell
New-Item -ItemType File src/server/pool/.ready
New-Item -ItemType File src/server/concurrency/.ready
```
