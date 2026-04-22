# Server Workspace Guide

서버 관련 구현은 이 폴더 아래 역할별 하위 폴더에서만 진행한다.

## 공통 원칙

- 각 개발자는 자신의 하위 폴더 `AGENTS.md`를 먼저 읽고 작업한다.
- 다른 역할 폴더의 파일은 임의로 수정하지 않는다.
- 공용 헤더 수정이 필요하면 담당자와 먼저 조율한다.

## 역할별 폴더

- `api/`: 서버 시작/종료, 소켓, 클라이언트 연결
- `pool/`: worker thread, queue, submit/pop
- `request/`: HTTP 요청 처리, SQL 서비스, 응답 변환
- `concurrency/`: read/write lock, DB 보호 계층
