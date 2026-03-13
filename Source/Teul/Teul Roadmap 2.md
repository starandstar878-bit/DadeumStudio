# Teul Roadmap 2

> 목적: `Teul` 리라이트 전에 최상위 구조를 단순하게 다시 고정한다.
> 이 문서는 세부 클래스 분해안보다, 시스템을 어떤 계층으로 나누고 각 계층이 무엇을 책임지는지에 집중한다.

---

## 최상위 구조

`Teul`은 우선 아래 4개 계층으로 나눈다.

```text
Source/Teul/
  Document/
  Editor/
  Runtime/
  Bridge/
```

### 1. Document
- 문서를 시스템에 맞는 형태로 받아들인다.
- 문서를 런타임 동안 관리한다.
- history snapshot을 관리하고 undo/redo를 수행한다.
- 버전을 검사하고 필요하면 upgrade/migration 한다.
- ValueTree 양식과 외부 직렬화 형식을 관리한다.
- 저장, 불러오기, autosave, recovery를 담당한다.

### 2. Editor
- 문서를 사람이 보고 수정하는 표현 계층이다.
- canvas, panel, inspector, rail, selection, drag, menu를 담당한다.
- 문서를 직접 소유하기보다 `Document` 계층의 현재 문서를 표시하고 편집한다.

### 3. Runtime
- 문서를 실행 가능한 graph로 바꾼다.
- audio, MIDI, control을 처리한다.
- 런타임 통계와 runtime-safe parameter apply를 담당한다.

### 4. Bridge
- Ieum 등 다른 프로젝트와의 연결을 담당한다.
- export, parameter surface, 외부 소비용 인터페이스를 담당한다.

---

## 1차 목표

리라이트 1차 목표는 세부 폴더를 많이 만드는 것이 아니라, 아래를 먼저 달성하는 것이다.

- `Document`, `Editor`, `Runtime`, `Bridge` 4계층 경계 고정
- `EditorHandleImpl`에서 orchestration 외 책임 제거 준비
- 현재 `Model`, `Serialization`, `History`에 흩어진 문서 책임을 `Document` 중심으로 재정리
- 나머지 세분화는 2차 작업으로 미룸

---

## Document 계층 역할

`Document`는 단순 데이터 보관소가 아니라 문서 lifecycle 전체를 책임지는 계층으로 본다.

핵심 책임은 아래와 같다.

- 현재 문서의 in-memory 상태 보유
- node/port/connection/control source/system endpoint 구조 정의
- history snapshot 저장
- undo/redo 수행
- revision/version 관리
- 구버전 문서 migration
- ValueTree 양식 관리
- 외부 직렬화/역직렬화
- 파일 저장/불러오기
- autosave/recovery 관리

즉, 문서가 들어오면 시스템이 이해할 수 있는 현재 포맷으로 바꾸고, 실행 중에는 그 문서를 관리하고, 필요할 때 다시 저장 가능한 형태로 내보내는 계층이다.

---

## Document 폴더 최소 파일 구조

```text
Source/Teul/Document/
  TDocumentTypes.h
  TTeulDocument.h
  TTeulDocument.cpp
  TDocumentHistory.h
  TDocumentHistory.cpp
  TDocumentSerializer.h
  TDocumentSerializer.cpp
  TDocumentMigration.h
  TDocumentMigration.cpp
  TDocumentStore.h
  TDocumentStore.cpp
```

### TDocumentTypes.h
- 문서 공통 타입 정의
- `Node`, `Port`, `Connection`, `ControlSource`, `SystemEndpoint`
- `DocumentVersion`, `DocumentId`, `RevisionId`
- ValueTree property key 상수

### TTeulDocument.h / TTeulDocument.cpp
- 정규화된 in-memory 문서 모델
- 현재 시스템이 실제로 사용하는 문서 객체
- revision 관리
- 문서 무결성 유지
- 기본 mutation API 제공

### TDocumentHistory.h / TDocumentHistory.cpp
- 문서 snapshot stack 관리
- undo / redo
- history depth 제한
- snapshot push/pop 정책 관리

### TDocumentSerializer.h / TDocumentSerializer.cpp
- `TTeulDocument <-> ValueTree`
- `TTeulDocument <-> JSON`
- 외부 저장 포맷과 내부 문서 포맷 간 변환

### TDocumentMigration.h / TDocumentMigration.cpp
- 문서 버전 검사
- 구버전 문서 upgrade
- migration step 적용
- load 시 현재 버전으로 정규화

### TDocumentStore.h / TDocumentStore.cpp
- 파일 입출력 facade
- 저장 / 불러오기 / autosave / recovery
- serializer, migration, history를 묶는 상위 진입점

---

## 현재 코드 기준 이동 방향

현재 흩어진 책임은 장기적으로 아래처럼 정리한다.

- `Model/`의 순수 문서 구조는 `Document/`로 이동
- `History/`의 undo/redo는 `DocumentHistory`로 흡수
- `Serialization/`의 문서 저장/불러오기는 `DocumentSerializer`, `DocumentStore` 중심으로 재구성
- migration 관련 책임은 `DocumentMigration`으로 통합

즉, 문서 관련 코드는 최종적으로 `Document` 폴더 하나에서 읽히게 만드는 것이 목표다.

---

## 이후 단계

이 문서 기준에서 다음 설계는 순서대로 진행한다.

1. `Document` 폴더 구조 확정
2. `Editor` 최소 구조 정의
3. `Runtime` 최소 구조 정의
4. `Bridge` 최소 구조 정의
5. 마지막에 각 계층 사이 공개 인터페이스 정리

---

## 완료 판정 기준

- 최상위 계층이 `Document`, `Editor`, `Runtime`, `Bridge` 4개로 설명 가능해야 한다.
- 문서 lifecycle 책임이 `Document` 계층 하나로 읽혀야 한다.
- undo/redo, migration, serialization, autosave가 문서 계층 책임으로 정리되어야 한다.
- `EditorHandleImpl`은 문서 처리 계층의 세부 구현을 직접 품지 않아야 한다.