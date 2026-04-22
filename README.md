# Jungle-W8-minisql

미니 SQL 엔진에 API 서버 계층을 붙인 8주차 프로젝트입니다. 기존 CLI SQL 실행기와 B+Tree 인덱스를 유지하면서, 외부 클라이언트가 HTTP `POST` 요청으로 `SELECT`/`INSERT`를 호출할 수 있도록 서버 계층을 추가했습니다.

## 구성

- `sql_processor`: 기존 CLI 실행기
- `sql_api_server`: HTTP API 서버 런타임
- `src/server/api/`: 소켓 수신, 서버 lifecycle
- `src/server/pool/`: thread pool, job queue
- `src/server/request/`: HTTP 파싱, 요청 검증, SQL 서비스 연결
- `src/server/concurrency/`: DB read/write 보호 계층

## CLI 실행

```sh
./sql_processor --query "SELECT * FROM users WHERE id = 2;" --db ./data
./sql_processor --sql tests/integration/smoke.sql --db ./data
./sql_processor --bench 1000000 --db ./data
```

## API 서버 실행

```sh
./sql_api_server --host 0.0.0.0 --port 8080 --db ./data
```

요청 형식은 `POST` + request body 입니다.

```sh
curl -X POST http://127.0.0.1:8080/query \
  -H "Content-Type: text/plain" \
  --data "SELECT * FROM users WHERE id = 2;"
```

예상 응답 본문:

```json
{"ok":true,"type":"select","table":"users","rows":[{"id":2,"name":"Bob","age":22}]}
```

## 테스트

```sh
make unit
make integration
make api-smoke
make test
```

- `unit`: parser, B+Tree, server config, HTTP protocol, SQL service, request handler 검증
- `integration`: 기존 CLI 기반 SQL roundtrip 검증
- `api-smoke`: API 서버를 띄운 뒤 `POST /query` smoke와 동시 SELECT/INSERT 병렬 처리 검증

## 발표 시연 순서

1. `sql_processor`로 기존 SQL 엔진 동작 시연
2. `sql_api_server` 실행
3. `curl` 또는 Postman으로 `POST /query` 요청 전송
4. `SELECT` 응답 확인
5. 필요하면 `INSERT` 후 재조회로 API-DB 연결 시연
