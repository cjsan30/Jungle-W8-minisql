# Edge Cases

프로젝트에서 신경 써야 하는 엣지케이스를 카테고리별로 정리한 문서입니다.

## 1. HTTP 입력 경계

### 현재 커버됨
- 정상 `POST /query` 요청 파싱
- 요청 라인 개행 누락
- 빈 path
- 직렬화된 HTTP 응답의 상태 코드, 헤더, body 길이

### 관련 테스트
- `tests/unit/test_http_protocol.c`
- `tests/integration/assert_api_select.sh`

### 아직 남음
- 잘못된 `Content-Length`
- body가 중간에 끊긴 요청
- 너무 큰 요청 payload
- 비정상 헤더 조합

## 2. API 요청 검증

### 현재 커버됨
- `GET` 요청 거부
- blank body 거부
- path가 `/`로 시작하지 않는 요청 거부
- unsupported SQL 거부

### 관련 테스트
- `tests/unit/test_request_handler.c`
- `tests/integration/assert_api_error_cases.sh`

### 아직 남음
- 허용되지 않은 `Content-Type`
- `/query` 외 세부 path 정책이 필요할 경우 그 검증

## 3. SQL 파싱/실행 오류

### 현재 커버됨
- empty SQL
- unsupported SQL (`DROP TABLE ...`)
- `WHERE id`에 정수가 아닌 값
- 존재하지 않는 WHERE column
- duplicate id
- VALUES 개수 불일치
  현재 executor 경로에는 로직이 있으나 전용 테스트는 아직 없음

### 관련 테스트
- `tests/unit/test_sql_service.c`
- `tests/integration/run_cli_integration.ps1`
- `tests/integration/assert_api_error_cases.sh`

### 아직 남음
- `VALUES count mismatch` 전용 테스트
- 테이블이 존재하지 않는 경우
- parser의 더 복잡한 malformed SQL 조합

## 4. 데이터/직렬화 경계

### 현재 커버됨
- CSV 필드에 쉼표가 포함된 값 저장
- 잘못 persisted 된 row 감지
- JSON string escaping helper

### 관련 테스트
- `tests/integration/run_cli_integration.ps1`
- `tests/unit/test_sql_service.c`

### 아직 남음
- 실제 API 응답에서 따옴표, 역슬래시, 개행이 포함된 row 직렬화
- 매우 긴 문자열 값

## 5. 동시성

### 현재 커버됨
- read-read 병렬 허용
- read/write, write/write 상호배제
- 동시 `SELECT`
- 동시 `INSERT`
- `SELECT`와 `INSERT` 혼합 요청 후 최종 row 일관성 확인

### 관련 테스트
- `tests/unit/test_db_guard.c`
- `tests/integration/assert_api_concurrency.sh`

### 아직 남음
- 같은 PK를 동시에 insert 하는 충돌 경쟁
- 장시간 reader flood 상황에서 writer starvation 여부
- 더 큰 worker 수/요청 수 기준 스트레스 테스트

## 6. 운영/환경

### 현재 커버됨
- git hooks 설치 검증
- benchmark smoke
- devcontainer 기반 API smoke

### 관련 테스트
- `tests/integration/assert_git_hooks_installed.sh`
- `tests/integration/assert_git_hooks_installed.ps1`
- `tests/integration/assert_benchmark_output.sh`

### 아직 남음
- Windows 방화벽/실제 외부 장비 접근 자동 검증
- 포트 충돌 상황 처리 검증
